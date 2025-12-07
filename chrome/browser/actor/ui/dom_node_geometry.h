// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_DOM_NODE_GEOMETRY_H_
#define CHROME_BROWSER_ACTOR_UI_DOM_NODE_GEOMETRY_H_

#include "base/types/expected.h"
#include "chrome/browser/actor/shared_types.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/gfx/geometry/point.h"

namespace actor::ui {

// LINT.IfChange(GetDomNodeResult)
// This enum is persisted in UMA logs. Do not change or reuse existing values.
enum class GetDomNodeResult {
  kSuccess = 0,
  kNoApcMainFrameData = 1,
  kNodeNotFoundInApc = 2,
  kNoGeometry = 3,
  kOffScreen = 4,
  kMaxValue = kOffScreen,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:GetDomNodeResult)

class DomNodeGeometry {
 public:
  using NodeGeomMap =
      absl::flat_hash_map<DomNode,
                          optimization_guide::proto::ContentAttributes>;
  ~DomNodeGeometry();

  static std::unique_ptr<DomNodeGeometry> InitFromApc(
      const optimization_guide::proto::AnnotatedPageContent& apc);

  // Resolves a DomNode to its center point coordinates.  GetDomNodeResult will
  // indicate an error.  Errors during construction are returned persistently
  // from GetDomNode().
  base::expected<gfx::Point, GetDomNodeResult> GetDomNode(
      const DomNode& node) const;

 private:
  base::expected<gfx::Point, GetDomNodeResult> InternalGetDomNode(
      const DomNode& node) const;
  explicit DomNodeGeometry(NodeGeomMap map);
  explicit DomNodeGeometry(GetDomNodeResult construction_error);

  std::optional<GetDomNodeResult> init_error_;  // Errors during initialization
  const NodeGeomMap node_map_;
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_DOM_NODE_GEOMETRY_H_
