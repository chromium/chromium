// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_COMMON_SIGNALS_TYPE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_COMMON_SIGNALS_TYPE_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/dbus/attestation/attestation_ca.pb.h"

namespace enterprise_connectors {

// Use Chrome OS' signals definition.
using SignalsType = ::attestation::DeviceTrustSignals;

}  // namespace enterprise_connectors

#else
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/proto/device_trust_attestation_ca.pb.h"

namespace enterprise_connectors {

// Use the browser signals definition.
using SignalsType = DeviceTrustSignals;

}  // namespace enterprise_connectors

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_COMMON_SIGNALS_TYPE_H_
