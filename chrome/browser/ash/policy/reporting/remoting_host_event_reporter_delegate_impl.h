// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_REMOTING_HOST_EVENT_REPORTER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_REMOTING_HOST_EVENT_REPORTER_DELEGATE_IMPL_H_

#include <memory>

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/crd_event.pb.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "remoting/host/chromeos/host_event_reporter_impl.h"

namespace remoting {

// Production implementation of CRD event reporter for remoting host.
class HostEventReporterDelegateImpl : public HostEventReporterImpl::Delegate {
 public:
  explicit HostEventReporterDelegateImpl(
      std::unique_ptr<::reporting::UserEventReporterHelper> user_event_helper =
          std::make_unique<::reporting::UserEventReporterHelper>(
              ::reporting::Destination::CRD_EVENTS,
              ::reporting::EventType::kUser));

  HostEventReporterDelegateImpl(const HostEventReporterDelegateImpl& other) =
      delete;
  HostEventReporterDelegateImpl& operator=(
      const HostEventReporterDelegateImpl& other) = delete;
  ~HostEventReporterDelegateImpl() override;

  void EnqueueEvent(::ash::reporting::CRDRecord record) override;

 private:
  // Helper class - refcounted in order to keep it alive until asynchronous
  // Enqueue task completes.
  class Helper;

  // Helper for posting events.
  const scoped_refptr<Helper> helper_;
};
}  // namespace remoting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_REMOTING_HOST_EVENT_REPORTER_DELEGATE_IMPL_H_
