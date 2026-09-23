// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SELECTION_SUGGESTION_H_
#define CHROME_BROWSER_SELECTION_SUGGESTION_H_

#include <string>
#include <variant>
#include <vector>

#include "chrome/browser/selection/mojom/action.mojom-forward.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace selection {

// Area of interest for the suggested service to examine.
struct AreaOfInterest {
  AreaOfInterest();
  AreaOfInterest(const AreaOfInterest&);
  AreaOfInterest& operator=(const AreaOfInterest&);
  AreaOfInterest(AreaOfInterest&&);
  AreaOfInterest& operator=(AreaOfInterest&&);
  ~AreaOfInterest();

  optimization_guide::proto::AnnotatedPageContent apc;
  SkBitmap screenshot;
  std::variant<gfx::Rect, std::vector<gfx::Point>> bounds;
};

class Suggestion {
 public:
  Suggestion();
  virtual ~Suggestion();

  // Returns the label for the suggestion.
  virtual const std::u16string& GetLabel() const = 0;

  // Called when the suggestion is presented.
  virtual void OnSuggestionPresented() = 0;

  // Called when the user accepts a suggestion associated with this tool.
  virtual void OnSuggestionExecuted() = 0;

  // Returns what executing this suggestion should do to the surface that
  // offered it.
  virtual mojom::ActionPtr GetAction() const = 0;
};

}  // namespace selection

#endif  // CHROME_BROWSER_SELECTION_SUGGESTION_H_

