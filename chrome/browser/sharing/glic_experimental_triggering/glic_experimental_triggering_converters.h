// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_CONVERTERS_H_
#define CHROME_BROWSER_SHARING_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_CONVERTERS_H_

#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_types.h"
#include "components/sharing_message/proto/glic_experimental_triggering.pb.h"
#include "components/sharing_message/proto/sharing_message.pb.h"

namespace glic {

// Converts a raw GlicExperimentalTriggering protobuf to a domain
// ExperimentalTriggeringRequest.
ExperimentalTriggeringRequest ProtoToRequest(
    const components_sharing_message::GlicExperimentalTriggering& proto);

// Converts a domain ExperimentalTriggeringResponse back into a SharingMessage
// protobuf for transmission via SharingMessageSender.
components_sharing_message::SharingMessage ResponseToProto(
    const ExperimentalTriggeringResponse& response);

}  // namespace glic

#endif  // CHROME_BROWSER_SHARING_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_CONVERTERS_H_
