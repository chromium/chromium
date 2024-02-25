// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/arc_ui_availability_reporter.h"

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/arc_optin_uma.h"

namespace arc {

class ArcUiAvailabilityReporter::ConnectionNotifierBase {
 public:
  ConnectionNotifierBase(const ConnectionNotifierBase&) = delete;
  ConnectionNotifierBase& operator=(const ConnectionNotifierBase&) = delete;

  virtual ~ConnectionNotifierBase() = default;

  // Returns true if connection is ready.
  virtual bool IsConnected() = 0;

 protected:
  explicit ConnectionNotifierBase(ArcUiAvailabilityReporter* owner)
      : owner_(owner) {}

  ArcUiAvailabilityReporter* owner() { return owner_; }

 private:
  const raw_ptr<ArcUiAvailabilityReporter> owner_;
};

namespace {

template <typename InstanceType, typename HostType>
class ConnectionNotifier
    : public ArcUiAvailabilityReporter::ConnectionNotifierBase,
      public ConnectionObserver<InstanceType> {
 public:
  // |owner_| owns this object and |ConnectionNotifier| is destroyed in case
  // statistics is reported or ARC session is terminated on user log-out and
  // this automatically destroys this object at time of destroying |owner_|.
  // |holder_| is owned by ArcBridgeService this lives longer than
  // |ArcUiAvailabilityReporter|.
  ConnectionNotifier(ArcUiAvailabilityReporter* owner,
                     ConnectionHolder<InstanceType, HostType>* holder)
      : ArcUiAvailabilityReporter::ConnectionNotifierBase(owner),
        holder_(holder) {
    DCHECK(!holder_->IsConnected());
    holder_->AddObserver(this);
  }

  ConnectionNotifier(const ConnectionNotifier&) = delete;
  ConnectionNotifier& operator=(const ConnectionNotifier&) = delete;

  ~ConnectionNotifier() override { holder_->RemoveObserver(this); }

  // ArcUiAvailabilityReporter::ConnectionNotifierBase:
  bool IsConnected() override { return holder_->IsConnected(); }

  // ConnectionObserver<InstanceType>:
  void OnConnectionReady() override { owner()->MaybeReport(); }

 private:
  const raw_ptr<ConnectionHolder<InstanceType, HostType>> holder_;
};

}  // namespace

ArcUiAvailabilityReporter::ArcUiAvailabilityReporter(Profile* profile,
                                                     Mode mode)
    : profile_(profile), mode_(mode), start_ticks_(base::TimeTicks::Now()) {
  DCHECK(profile);
  // Some unit tests may not have |ArcServiceManager| set.
  ArcServiceManager* const service_manager = ArcServiceManager::Get();
  if (!service_manager)
    return;
  // |initiated_from_oobe| must be set only if |initial_start| is set.
  auto* const arc_bridge = ArcServiceManager::Get()->arc_bridge_service();
  // Not expected instances are connected at the time of creation, however this
  // is not always preserved in tests.
  if (arc_bridge->app()->IsConnected() ||
      arc_bridge->intent_helper()->IsConnected()) {
    LOG(ERROR) << "App and/or intent_helper instances are already connected. "
               << "This is not expected in production.";
    return;
  }

  connection_notifiers_.emplace_back(
      std::make_unique<ConnectionNotifier<mojom::AppInstance, mojom::AppHost>>(
          this, arc_bridge->app()));
  connection_notifiers_.emplace_back(
      std::make_unique<ConnectionNotifier<mojom::IntentHelperInstance,
                                          mojom::IntentHelperHost>>(
          this, arc_bridge->intent_helper()));
}

ArcUiAvailabilityReporter::~ArcUiAvailabilityReporter() = default;

// static
std::string ArcUiAvailabilityReporter::GetHistogramNameForMode(Mode mode) {
  switch (mode) {
    case Mode::kOobeProvisioning:
      return "OobeProvisioning";
    case Mode::kInSessionProvisioning:
      return "InSessionProvisioning";
    case Mode::kAlreadyProvisioned:
      return "AlreadyProvisioned";
  }
}

void ArcUiAvailabilityReporter::MaybeReport() {
  DCHECK(!connection_notifiers_.empty());

  // Check that all tracked instance are connected.
  for (const auto& connection_notifier : connection_notifiers_) {
    if (!connection_notifier->IsConnected())
      return;
  }

  UpdateArcUiAvailableTime(base::TimeTicks::Now() - start_ticks_,
                           GetHistogramNameForMode(mode_), profile_);

  // No more reporting is expected.
  connection_notifiers_.clear();
}

}  // namespace arc
