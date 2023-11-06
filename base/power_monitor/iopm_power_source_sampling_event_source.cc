// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/iopm_power_source_sampling_event_source.h"

#include <IOKit/IOMessage.h>
#include <dispatch/queue.h>

#include "base/check.h"
#include "base/logging.h"

namespace base {

IOPMPowerSourceSamplingEventSource::IOPMPowerSourceSamplingEventSource() =
    default;

IOPMPowerSourceSamplingEventSource::~IOPMPowerSourceSamplingEventSource() =
    default;

bool IOPMPowerSourceSamplingEventSource::Start(SamplingEventCallback callback) {
  DCHECK(!callback_);
  DCHECK(callback);

  callback_ = callback;

  service_.reset(IOServiceGetMatchingService(
      kIOMasterPortDefault, IOServiceMatching("IOPMPowerSource")));

  if (!service_) {
    VLOG(1) << "IOPMPowerSource service not found. This is expected on desktop "
               "Macs.";
    return false;
  }

  notify_port_.reset(IONotificationPortCreate(kIOMasterPortDefault));
  if (!notify_port_.is_valid()) {
    LOG(ERROR) << "Could not create a notification port";
    return false;
  }

  IONotificationPortSetDispatchQueue(notify_port_.get(),
                                     dispatch_get_main_queue());

  kern_return_t result = IOServiceAddInterestNotification(
      notify_port_.get(), service_.get(), kIOGeneralInterest, OnNotification,
      this, notification_.InitializeInto());

  if (result != KERN_SUCCESS) {
    LOG(ERROR) << "Could not register to IOPMPowerSource notifications";
    return false;
  }

  return true;
}

// static
void IOPMPowerSourceSamplingEventSource::OnNotification(
    void* context,
    io_service_t service,
    natural_t message_type,
    void* message_argument) {
  IOPMPowerSourceSamplingEventSource* self =
      static_cast<IOPMPowerSourceSamplingEventSource*>(context);
  self->callback_.Run();
}

}  // namespace base
