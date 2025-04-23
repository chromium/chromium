// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/apple/mach_port_rendezvous_mac.h"

#include <bsm/libbsm.h>
#include <mach/mig.h>
#include <servers/bootstrap.h>
#include <unistd.h>

#include <utility>

#include "base/apple/foundation_util.h"
#include "base/apple/mach_logging.h"
#include "base/apple/scoped_dispatch_object.h"
#include "base/bits.h"
#include "base/compiler_specific.h"
#include "base/containers/buffer_iterator.h"
#include "base/environment.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/mac/info_plist_data.h"
#include "base/mac/scoped_mach_msg_destroy.h"
#include "base/no_destructor.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/types/cxx23_to_underlying.h"

namespace base {

// Whether any peer process requirements should be validated.
BASE_FEATURE(kMachPortRendezvousValidatePeerRequirements,
             "MachPortRendezvousValidatePeerRequirements",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether a failure to validate a peer process against a requirement
// should result in aborting the rendezvous.
BASE_FEATURE(kMachPortRendezvousEnforcePeerRequirements,
             "MachPortRendezvousEnforcePeerRequirements",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

// The name to use in the bootstrap server, formatted with the BaseBundleID and
// PID of the server.
constexpr char kBootstrapNameFormat[] = "%s.MachPortRendezvousServer.%d";

// This can be safely increased if Info.plist grows in the future.
constexpr size_t kMaxInfoPlistDataSize = 18 * 1024;

// The state of the peer validation policy features is passed to child processes
// via this environment variable as Mach port rendezvous is performed before the
// feature list is initialized.
// TODO(crbug.com/362302761): Remove once enforcement is enabled by default.
constexpr char kPeerValidationPolicyEnvironmentVariable[] =
    "MACH_PORT_RENDEZVOUS_PEER_VALDATION";

MachPortRendezvousPeerValidationPolicy GetPeerValidationPolicy();

bool ShouldValidateProcessRequirements() {
  return GetPeerValidationPolicy() !=
         MachPortRendezvousPeerValidationPolicy::kNoValidation;
}

bool ShouldEnforceProcessRequirements() {
  return GetPeerValidationPolicy() ==
         MachPortRendezvousPeerValidationPolicy::kEnforce;
}

}  // namespace

struct MachPortRendezvousServerMac::ClientData {
  ClientData();
  ClientData(ClientData&&);
  ~ClientData();

  // A DISPATCH_SOURCE_TYPE_PROC / DISPATCH_PROC_EXIT dispatch source. When
  // the source is triggered, it calls OnClientExited().
  apple::ScopedDispatchObject<dispatch_source_t> exit_watcher;

  MachPortsForRendezvous ports;
  std::optional<mac::ProcessRequirement> requirement;
};

// static
MachPortRendezvousServerMac* MachPortRendezvousServerMac::GetInstance() {
  static auto* instance = new MachPortRendezvousServerMac();
  return instance;
}

// static
void MachPortRendezvousServerMac::AddFeatureStateToEnvironment(
    EnvironmentMap& environment) {
  environment.insert(
      {kPeerValidationPolicyEnvironmentVariable,
       NumberToString(static_cast<int>(GetPeerValidationPolicy()))});
}

MachPortRendezvousServerMac::ClientData&
MachPortRendezvousServerMac::ClientDataForPid(pid_t pid) {
  lock_.AssertAcquired();

  auto [it, inserted] = client_data_.emplace(pid, ClientData{});
  if (inserted) {
    apple::ScopedDispatchObject<dispatch_source_t> exit_watcher(
        dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC,
                               static_cast<uintptr_t>(pid), DISPATCH_PROC_EXIT,
                               dispatch_source_->Queue()));
    dispatch_source_set_event_handler(exit_watcher.get(), ^{
      OnClientExited(pid);
    });
    dispatch_resume(exit_watcher.get());
    it->second.exit_watcher = std::move(exit_watcher);
  }

  return it->second;
}

void MachPortRendezvousServerMac::RegisterPortsForPid(
    pid_t pid,
    const MachPortsForRendezvous& ports) {
  lock_.AssertAcquired();
  DCHECK_LT(ports.size(), internal::kMaximumRendezvousPorts);
  DCHECK(!ports.empty());

  ClientData& client = ClientDataForPid(pid);
  CHECK(client.ports.empty());
  client.ports = ports;
}

void MachPortRendezvousServerMac::SetProcessRequirementForPid(
    pid_t pid,
    mac::ProcessRequirement requirement) {
  lock_.AssertAcquired();

  ClientData& client = ClientDataForPid(pid);
  CHECK(!client.requirement.has_value());
  client.requirement = std::move(requirement);
}

MachPortRendezvousServerMac::ClientData::ClientData() = default;
MachPortRendezvousServerMac::ClientData::ClientData(ClientData&&) = default;

MachPortRendezvousServerMac::ClientData::~ClientData() {
  for (auto& pair : ports) {
    pair.second.Destroy();
  }
}

MachPortRendezvousServerMac::MachPortRendezvousServerMac() {
  std::string bootstrap_name =
      StringPrintf(kBootstrapNameFormat, apple::BaseBundleID(), getpid());
  kern_return_t kr = bootstrap_check_in(
      bootstrap_port, bootstrap_name.c_str(),
      apple::ScopedMachReceiveRight::Receiver(server_port_).get());
  BOOTSTRAP_CHECK(kr == KERN_SUCCESS, kr)
      << "bootstrap_check_in " << bootstrap_name;
  dispatch_source_ = std::make_unique<apple::DispatchSource>(
      bootstrap_name.c_str(), server_port_.get(), ^{
        HandleRequest();
      });
  dispatch_source_->Resume();
}

MachPortRendezvousServerMac::~MachPortRendezvousServerMac() = default;

void MachPortRendezvousServerMac::ClearClientDataForTesting() {
  client_data_.clear();
}

std::optional<MachPortsForRendezvous>
MachPortRendezvousServerMac::PortsForClient(audit_token_t audit_token) {
  pid_t pid = audit_token_to_pid(audit_token);
  std::optional<mac::ProcessRequirement> requirement;
  MachPortsForRendezvous ports_to_send;

  {
    AutoLock lock(lock_);
    auto it = client_data_.find(pid);
    if (it != client_data_.end()) {
      ports_to_send = std::move(it->second.ports);
      requirement = std::move(it->second.requirement);
      client_data_.erase(it);
    }
  }

  if (requirement.has_value() && ShouldValidateProcessRequirements()) {
    bool client_is_valid = requirement->ValidateProcess(audit_token);
    if (!client_is_valid && ShouldEnforceProcessRequirements()) {
      return std::nullopt;
    }
  }

  return ports_to_send;
}

void MachPortRendezvousServerMac::OnClientExited(pid_t pid) {
  AutoLock lock(lock_);
  client_data_.erase(pid);
}

bool MachPortRendezvousServerMac::IsValidAdditionalMessageId(
    mach_msg_id_t request) const {
  return request == internal::kMachRendezvousMsgIdRequestWithInfoPlistData;
}

std::vector<uint8_t> MachPortRendezvousServerMac::AdditionalDataForReply(
    mach_msg_id_t request) const {
  if (request == internal::kMachRendezvousMsgIdRequestWithInfoPlistData) {
    std::vector<uint8_t> info_plist_data =
        mac::OuterBundleCachedInfoPlistData();
    if (info_plist_data.size() > kMaxInfoPlistDataSize) {
      LOG(WARNING) << "Info.plist data too large to send to client.";
      return {};
    }
    return info_plist_data;
  }
  return {};
}

// static
MachPortRendezvousClient* MachPortRendezvousClient::GetInstance() {
  static MachPortRendezvousClientMac* client = []() -> auto* {
    auto* client = new MachPortRendezvousClientMac();
    if (!client->AcquirePorts()) {
      delete client;
      client = nullptr;
    }
    return client;
  }();
  return client;
}

// static
std::string MachPortRendezvousClientMac::GetBootstrapName() {
  return StringPrintf(kBootstrapNameFormat, apple::BaseBundleID(), getppid());
}

MachPortRendezvousClientMac::MachPortRendezvousClientMac()
    : server_requirement_(TakeServerCodeSigningRequirement()) {}

MachPortRendezvousClientMac::~MachPortRendezvousClientMac() = default;

bool MachPortRendezvousClientMac::AcquirePorts() {
  AutoLock lock(lock_);

  apple::ScopedMachSendRight server_port;
  std::string bootstrap_name = GetBootstrapName();
  kern_return_t kr = bootstrap_look_up(
      bootstrap_port, const_cast<char*>(bootstrap_name.c_str()),
      apple::ScopedMachSendRight::Receiver(server_port).get());
  if (kr != KERN_SUCCESS) {
    BOOTSTRAP_LOG(ERROR, kr) << "bootstrap_look_up " << bootstrap_name;
    return false;
  }

  mach_msg_id_t message_id = internal::kMachRendezvousMsgIdRequest;
  size_t additional_data_size = 0;
  if (NeedsInfoPlistData()) {
    message_id = internal::kMachRendezvousMsgIdRequestWithInfoPlistData;
    additional_data_size = kMaxInfoPlistDataSize;
  }
  return SendRequest(std::move(server_port), message_id, additional_data_size);
}

bool MachPortRendezvousClientMac::ValidateMessage(
    mach_msg_base_t* message,
    BufferIterator<uint8_t> iterator) {
  if (!server_requirement_.has_value() ||
      !ShouldValidateProcessRequirements()) {
    return true;
  }

  span<const uint8_t> info_plist_data;
  if (NeedsInfoPlistData()) {
    // Skip over the Mach ports to find the Info.plist data to use for
    // validation.
    iterator.Seek(iterator.position() +
                  message->body.msgh_descriptor_count *
                      (sizeof(mach_msg_port_descriptor_t) +
                       sizeof(MachPortsForRendezvous::key_type)));
    auto info_plist_length = iterator.CopyObject<uint64_t>();
    CHECK(info_plist_length.has_value());
    CHECK(*info_plist_length <= kMaxInfoPlistDataSize);
    info_plist_data = iterator.Span<uint8_t>(info_plist_length.value());
  }

  iterator.Seek(message->header.msgh_size);
  auto* trailer = iterator.Object<mach_msg_audit_trailer_t>();
  bool valid = server_requirement_->ValidateProcess(trailer->msgh_audit,
                                                    info_plist_data);
  if (ShouldEnforceProcessRequirements()) {
    return valid;
  }
  return true;
}

bool MachPortRendezvousClientMac::NeedsInfoPlistData() const {
  return ShouldValidateProcessRequirements() &&
         server_requirement_.has_value() &&
         server_requirement_->ShouldCheckDynamicValidityOnly();
}

namespace {

struct RequirementWithLock {
  Lock lock;
  std::optional<mac::ProcessRequirement> requirement;
};

RequirementWithLock& ServerCodeSigningRequirementWithLock() {
  static NoDestructor<RequirementWithLock> requirement_with_lock;
  return *requirement_with_lock;
}

}  // namespace

// static
void MachPortRendezvousClientMac::SetServerProcessRequirement(
    mac::ProcessRequirement requirement) {
  AutoLock lock(ServerCodeSigningRequirementWithLock().lock);
  ServerCodeSigningRequirementWithLock().requirement = requirement;
}

// static
std::optional<mac::ProcessRequirement>
MachPortRendezvousClientMac::TakeServerCodeSigningRequirement() {
  AutoLock lock(ServerCodeSigningRequirementWithLock().lock);
  return std::move(ServerCodeSigningRequirementWithLock().requirement);
}

// static
MachPortRendezvousPeerValidationPolicy
MachPortRendezvousClientMac::PeerValidationPolicyForTesting() {
  return GetPeerValidationPolicy();
}

namespace {

// Helper function to avoid the compiler detecting that comparisons involving
// `default_state` are compile-time constants and declaring code as unreachable.
bool IsEnabledByDefault(const Feature& feature) {
  return feature.default_state == FEATURE_ENABLED_BY_DEFAULT;
}

MachPortRendezvousPeerValidationPolicy GetDefaultPeerValidationPolicy() {
  CHECK(!FeatureList::GetInstance());
  if (IsEnabledByDefault(kMachPortRendezvousEnforcePeerRequirements)) {
    return MachPortRendezvousPeerValidationPolicy::kEnforce;
  }
  if (IsEnabledByDefault(kMachPortRendezvousValidatePeerRequirements)) {
    return MachPortRendezvousPeerValidationPolicy::kValidateOnly;
  }
  return MachPortRendezvousPeerValidationPolicy::kNoValidation;
}

MachPortRendezvousPeerValidationPolicy
GetPeerValidationPolicyFromFeatureList() {
  if (base::FeatureList::IsEnabled(
          kMachPortRendezvousEnforcePeerRequirements)) {
    return MachPortRendezvousPeerValidationPolicy::kEnforce;
  }
  if (base::FeatureList::IsEnabled(
          kMachPortRendezvousValidatePeerRequirements)) {
    return MachPortRendezvousPeerValidationPolicy::kValidateOnly;
  }
  return MachPortRendezvousPeerValidationPolicy::kNoValidation;
}

MachPortRendezvousPeerValidationPolicy
GetPeerValidationPolicyFromEnvironment() {
  // The environment variable is set at launch and does not change. Compute the
  // policy once and cache it.
  static MachPortRendezvousPeerValidationPolicy policy = [] {
    std::unique_ptr<Environment> environment = Environment::Create();
    int policy_int = INT_MAX;
    std::optional<std::string> policy_str =
        environment->GetVar(kPeerValidationPolicyEnvironmentVariable);
    if (policy_str.has_value()) {
      if (!StringToInt(policy_str.value(), &policy_int)) {
        // StringToInt modifies the output value even on failure.
        policy_int = INT_MAX;
      }
    }

    switch (policy_int) {
      case to_underlying(MachPortRendezvousPeerValidationPolicy::kNoValidation):
        return MachPortRendezvousPeerValidationPolicy::kNoValidation;
      case to_underlying(MachPortRendezvousPeerValidationPolicy::kValidateOnly):
        return MachPortRendezvousPeerValidationPolicy::kValidateOnly;
      case to_underlying(MachPortRendezvousPeerValidationPolicy::kEnforce):
        return MachPortRendezvousPeerValidationPolicy::kEnforce;
      default:
        // An invalid policy or no policy was passed via the environment. Fall
        // back to the default values of the feature flags.
        return GetDefaultPeerValidationPolicy();
    }
  }();
  return policy;
}

MachPortRendezvousPeerValidationPolicy GetPeerValidationPolicy() {
  if (base::FeatureList::GetInstance()) {
    return GetPeerValidationPolicyFromFeatureList();
  }

  // In child processes, MachPortRendezvousClient is used during feature list
  // initialization so the validation policy is passed via an environment
  // variable.
  return GetPeerValidationPolicyFromEnvironment();
}

}  // namespace

}  // namespace base
