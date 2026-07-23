// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_ACTOR_GLIC_ACTOR_TEST_UTIL_H_
#define CHROME_BROWSER_GLIC_ACTOR_GLIC_ACTOR_TEST_UTIL_H_

#include <string>

#include "base/containers/flat_map.h"
#include "components/actor/core/shared_types.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace actor {
struct TaskSourceInfo;
}  // namespace actor

namespace glic {

// Returns a common mock TaskSourceInfo used by actor tests.
const ::actor::TaskSourceInfo& MockGlicTaskSourceInfo();

// Builds a map of labels to DomNodes from AnnotatedPageContent.
base::flat_map<std::string, ::actor::DomNode> BuildFormLabelsMap(
    const ::optimization_guide::proto::AnnotatedPageContent& apc);

// Returns a debug string representation of the form labels map.
std::string FormLabelsDebugString(
    const base::flat_map<std::string, ::actor::DomNode>& map);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_ACTOR_GLIC_ACTOR_TEST_UTIL_H_
