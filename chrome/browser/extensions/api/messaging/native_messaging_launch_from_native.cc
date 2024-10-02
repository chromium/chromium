// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_messaging_launch_from_native.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/messaging/native_message_port.h"
#include "chrome/browser/extensions/api/messaging/native_message_process_host.h"
#include "chrome/browser/extensions/api/messaging/native_process_launcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/manifest_handlers/natively_connectable_handler.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/messaging/channel_endpoint.h"
#include "extensions/browser/api/messaging/message_service.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/messaging/messaging_endpoint.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {
namespace {

ScopedAllowNativeAppConnectionForTest* g_allow_native_app_connection_for_test =
    nullptr;

constexpr base::TimeDelta kNativeMessagingHostErrorTimeout = base::Seconds(10);

ScopedNativeMessagingErrorTimeoutOverrideForTest*
    g_native_messaging_host_timeout_override = nullptr;

// A self-owning class responsible for starting a native messaging host with
// command-line parameters reporting an error, keeping Chrome alive until the
// host terminates, or a timeout is reached.
//
// This lives on the IO thread, but its public factory static method should be
// called on the UI thread.
class NativeMessagingHostErrorReporter : public NativeMessageHost::Client {
 public:
  using MovableScopedKeepAlive =
      std::unique_ptr<ScopedKeepAlive,
                      content::BrowserThread::DeleteOnUIThread>;

  NativeMessagingHostErrorReporter(const NativeMessagingHostErrorReporter&) =
      delete;
  NativeMessagingHostErrorReporter& operator=(
      const NativeMessagingHostErrorReporter&) = delete;

  static void Report(const ExtensionId& extension_id,
                     const std::string& host_id,
                     const std::string& connection_id,
                     Profile* profile,
                     const std::string& error_arg) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    std::unique_ptr<NativeMessageHost> host =
        NativeMessageProcessHost::CreateWithLauncher(
            extension_id, host_id,
            NativeProcessLauncher::CreateDefault(
                /* allow_user_level = */ true,
                /* native_view = */ nullptr, profile->GetPath(),
                /* require_native_initiated_connections = */ false,
                connection_id, error_arg, profile));
    MovableScopedKeepAlive keep_alive(
        new ScopedKeepAlive(KeepAliveOrigin::NATIVE_MESSAGING_HOST_ERROR_REPORT,
                            KeepAliveRestartOption::DISABLED));
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&NativeMessagingHostErrorReporter::ReportOnIoThread,
                       std::move(host), std::move(keep_alive)));
  }

 private:
  static void ReportOnIoThread(std::unique_ptr<NativeMessageHost> process,
                               MovableScopedKeepAlive keep_alive) {
    new NativeMessagingHostErrorReporter(std::move(process),
                                         std::move(keep_alive));
  }

  NativeMessagingHostErrorReporter(std::unique_ptr<NativeMessageHost> process,
                                   MovableScopedKeepAlive keep_alive)
      : keep_alive_(std::move(keep_alive)), process_(std::move(process)) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    timeout_.Start(
        FROM_HERE,
        g_native_messaging_host_timeout_override
            ? g_native_messaging_host_timeout_override->timeout()
            : kNativeMessagingHostErrorTimeout,
        base::BindOnce(&base::DeletePointer<NativeMessagingHostErrorReporter>,
                       base::Unretained(this)));
    process_->Start(this);
  }

 private:
  // NativeMessageHost::Client:
  void PostMessageFromNativeHost(const std::string& message) override {}

  void CloseChannel(const std::string& error_message) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    timeout_.AbandonAndStop();

    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE, this);
  }

  MovableScopedKeepAlive keep_alive_;
  std::unique_ptr<NativeMessageHost> process_;
  base::OneShotTimer timeout_;
};

}  // namespace

bool ExtensionSupportsConnectionFromNativeApp(const ExtensionId& extension_id,
                                              const std::string& host_id,
                                              Profile* profile,
                                              bool log_errors) {
  if (g_allow_native_app_connection_for_test) {
    return g_allow_native_app_connection_for_test->allow();
  }
  if (profile->IsOffTheRecord()) {
    return false;
  }
  auto* extension =
      ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(
          extension_id);
  if (!extension) {
    LOG_IF(ERROR, log_errors)
        << "Failed to launch native messaging connection: Unknown extension ID "
        << extension_id;
    return false;
  }
  const auto* natively_connectable_hosts =
      NativelyConnectableHosts::GetConnectableNativeMessageHosts(*extension);
  if (!natively_connectable_hosts ||
      !natively_connectable_hosts->count(host_id)) {
    LOG_IF(ERROR, log_errors)
        << "Extension \"" << extension_id << "\" does not list \"" << host_id
        << "\" in its natively_connectable manifest field";
    return false;
  }
  if (!extension->permissions_data()->active_permissions().HasAPIPermission(
          "nativeMessaging")) {
    LOG_IF(ERROR, log_errors)
        << "Extension \"" << extension_id
        << "\" does not have the \"nativeMessaging\" permission";
    return false;
  }
  if (!extension->permissions_data()->active_permissions().HasAPIPermission(
          "transientBackground")) {
    LOG_IF(ERROR, log_errors)
        << "Extension \"" << extension_id
        << "\" does not have the \"transientBackground\" permission";
    return false;
  }
  if (!EventRouter::Get(profile)->ExtensionHasEventListener(
          extension_id, "runtime.onConnectNative")) {
    LOG_IF(ERROR, log_errors)
        << "Failed to launch native messaging connection: Extension \""
        << extension_id << "\" is not listening for runtime.onConnectNative";
    return false;
  }

  return true;
}

ScopedAllowNativeAppConnectionForTest::ScopedAllowNativeAppConnectionForTest(
    bool allow)
    : allow_(allow) {
  DCHECK(!g_allow_native_app_connection_for_test);
  g_allow_native_app_connection_for_test = this;
}

ScopedAllowNativeAppConnectionForTest::
    ~ScopedAllowNativeAppConnectionForTest() {
  DCHECK_EQ(this, g_allow_native_app_connection_for_test);
  g_allow_native_app_connection_for_test = nullptr;
}

ScopedNativeMessagingErrorTimeoutOverrideForTest::
    ScopedNativeMessagingErrorTimeoutOverrideForTest(base::TimeDelta timeout)
    : timeout_(timeout) {
  DCHECK(!g_native_messaging_host_timeout_override);
  g_native_messaging_host_timeout_override = this;
}

ScopedNativeMessagingErrorTimeoutOverrideForTest::
    ~ScopedNativeMessagingErrorTimeoutOverrideForTest() {
  DCHECK_EQ(this, g_native_messaging_host_timeout_override);
  g_native_messaging_host_timeout_override = nullptr;
}

bool IsValidConnectionId(std::string_view connection_id) {
  return connection_id.size() <= 20 &&
         base::ContainsOnlyChars(
             connection_id,
             "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-");
}

void LaunchNativeMessageHostFromNativeApp(const ExtensionId& extension_id,
                                          const std::string& host_id,
                                          const std::string& connection_id,
                                          Profile* profile) {
  if (!IsValidConnectionId(connection_id)) {
    NativeMessagingHostErrorReporter::Report(extension_id, host_id,
                                             /* connect_id = */ {}, profile,
                                             "--invalid-connect-id");
    return;
  }
  if (!ExtensionSupportsConnectionFromNativeApp(extension_id, host_id, profile,
                                                /* log_errors = */ true)) {
    NativeMessagingHostErrorReporter::Report(extension_id, host_id,
                                             connection_id, profile,
                                             "--extension-not-installed");
    return;
  }
  const extensions::PortId port_id(
      base::UnguessableToken::Create(), 1 /* port_number */,
      true /* is_opener */, extensions::mojom::SerializationFormat::kJson);
  extensions::MessageService* const message_service =
      extensions::MessageService::Get(profile);
  // TODO(crbug.com/41461105): Apply policy for allow_user_level.
  auto native_message_host = NativeMessageProcessHost::CreateWithLauncher(
      extension_id, host_id,
      NativeProcessLauncher::CreateDefault(
          /* allow_user_level = */ true, /* native_view = */ nullptr,
          profile->GetPath(),
          /* require_native_initiated_connections = */ true, connection_id, "",
          profile));
  auto native_message_port = std::make_unique<extensions::NativeMessagePort>(
      message_service->GetChannelDelegate(), port_id,
      std::move(native_message_host));
  message_service->OpenChannelToExtension(
      extensions::ChannelEndpoint(profile), port_id,
      extensions::MessagingEndpoint::ForNativeApp(host_id),
      std::move(native_message_port), extension_id, GURL(),
      mojom::ChannelType::kNative, std::string() /* channel_name */);
}

}  // namespace extensions
