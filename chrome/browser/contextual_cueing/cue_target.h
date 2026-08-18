// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CUE_TARGET_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CUE_TARGET_H_

#include <optional>
#include <string>
#include <variant>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "ui/base/models/image_model.h"

namespace content {
class WebContents;
}  // namespace content

namespace page_content_annotations {
class PageContentAnnotationsResult;
}  // namespace page_content_annotations

namespace contextual_cueing {

// LINT.IfChange(CueTargetType)
enum class CueTargetType {
  kGlic = 0,
  kTestSource = 1,
  kMaxValue = kTestSource
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_cueing/enums.xml:CueTargetType)

// Level of UI prominence the orchestrator is willing to grant for this page
// load. Determined by the controller based on global quotas and backoff state.
enum class CueIntrusiveness {
  kLoud,   // Anchored message (subject to quiet loads constraint).
  kQuiet,  // Chip/icon (bypasses quiet loads constraint).
};

const char* GetName(CueTargetType type);

// Glic-specific click data. Conceptually this struct should carry everything
// needed for invoking Glic from a contextual cue.
struct GlicCueActionData {
  // Optional prompt to be filled in to glic upon opening.
  std::string prompt;
  // Tabs that should be used as context ("pinned") by glic.
  std::vector<tabs::TabHandle> tabs_to_share;

  GlicCueActionData();
  ~GlicCueActionData();
  GlicCueActionData(const GlicCueActionData&);
  GlicCueActionData(GlicCueActionData&&);
  GlicCueActionData& operator=(const GlicCueActionData&);
};

using CueActionData = std::variant<std::monostate, GlicCueActionData>;

struct CueAction {
  CueActionData data;
  std::vector<tabs::TabHandle> tabs_to_show;
};

// A feature that can show contextual cues to the user.
class CueTarget {
 public:
  virtual ~CueTarget() = default;

  // Returns the unique identifier for this target. Used as the key for UCB
  // scoring, backoff tracking, and metrics (impressions, clicks, dismissals).
  virtual CueTargetType GetType() const = 0;

  // Synchronous profile-level gate (e.g., feature disabled, panel already
  // open). Called by the controller before constructing the async barrier.
  //
  // TODO(b/523306363): Remove from the public interface once the legacy
  // single-source path in ContextualCueingController is fully migrated. Targets
  // should perform this check internally at the start of CheckEligibility().
  virtual bool IsEligible() const = 0;

  // Callback invoked when content generation completes (successfully or not).
  using GenerateCallback = base::OnceCallback<void(
      std::optional<optimization_guide::proto::ContextualCue>)>;

  // A generator closure provided by targets that produce content locally
  // (without MES). When run, it executes the target's local generation logic
  // and returns the result via GenerateCallback.
  using ContentGenerator = base::OnceCallback<void(GenerateCallback)>;

  // Evaluates whether this target can show a cue on the current page.
  //
  // Called at page navigation/load for all non-backed-off targets in
  // parallel via base::BarrierCallback. The controller passes in the
  // highest UI prominence tier currently allowed by global quotas:
  //
  //   - eligible: true if this target has relevant content for the page at
  //               the requested prominence level.
  //   - generator: optional generator closure. If non-null, the controller
  //                will invoke it to produce content if the framework decides
  //                to show the cue for this target. If null (and eligible is
  //                true), the controller will centrally generate content.
  //
  // May be asynchronous (e.g., waiting for page content annotations).
  // Implementations must safely handle `web_contents` being destroyed during
  // async execution.
  using EligibilityCallback =
      base::OnceCallback<void(bool eligible, ContentGenerator generator)>;
  virtual void CheckEligibility(
      base::WeakPtr<content::WebContents> web_contents,
      CueIntrusiveness intrusiveness,
      EligibilityCallback callback) = 0;

  // Synchronous page-level gate (e.g., page content annotations, URL checks).
  // Called by the controller before generating content in the legacy path.
  //
  // TODO(b/523306363): Remove from the public interface once the legacy
  // single-source path in ContextualCueingController is fully migrated. Targets
  // should perform this check internally in CheckEligibility().
  virtual bool IsPageEligible(
      const page_content_annotations::PageContentAnnotationsResult& result,
      content::WebContents* active_web_contents) const = 0;

  // Called when the user clicks the cue's action button.
  virtual void OnClick(CueActionData data) = 0;

  // Called when the user clicks the "edit prompt" menu item.
  virtual void OnEditPrompt(CueActionData data) = 0;

  // Icon to be shown in the anchored message.
  virtual ui::ImageModel GetAnchoredMessageIcon() const = 0;

  // Icon to be shown in the omnibox chip.
  virtual ui::ImageModel GetOmniboxChipIcon() const = 0;

  // Extract this feature's click data from a contextual cue.
  virtual CueActionData CueActionDataFromResponse(
      const optimization_guide::proto::ContextualCue& cue,
      std::vector<tabs::TabHandle> tabs_to_show) const = 0;

  virtual optimization_guide::proto::ContextualCueingSurface GetSurface()
      const = 0;
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CUE_TARGET_H_
