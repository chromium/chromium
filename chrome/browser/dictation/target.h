// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_TARGET_H_
#define CHROME_BROWSER_DICTATION_TARGET_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/dictation/metrics.h"
#include "content/public/browser/global_dom_node_id.h"

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace dictation {

class DictationContextFetcher;
enum class DictationStreamEndTrigger;
class DomTarget;

// Represents a dictation target into which transcriptions will be written,
// along with metadata and lifecycle/input hooks for the target's host surface.
class Target {
 public:
  // Handles lifecycle and user-input requests from the target surface.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when user interaction on the target surface should end the active
    // stream (e.g. pressing Escape, Enter, typing text, or changing focus).
    virtual void OnTargetRequestEndStream(
        DictationStreamEndTrigger trigger) = 0;

    // Called when user interaction on the target surface should end the whole
    // session (e.g. pressing Escape when no stream is active).
    virtual void OnTargetRequestEndSession(
        DictationStreamEndTrigger trigger) = 0;

    // Called when the underlying target surface is invalidated (e.g. navigated
    // away, crashed, or destroyed).
    virtual void OnTargetInvalidated() = 0;
  };

  virtual ~Target() = default;

  // Sets the composition text in the target.
  virtual void SetComposition(const std::u16string& text, bool is_final) = 0;

  // Commits the text in the target, invoking `on_commit_complete`
  // asynchronously once committed.
  virtual void CommitComposition(const std::u16string& text,
                                 base::OnceClosure on_commit_complete) = 0;

  // Returns the host tab associated with this target, or nullptr if unavailable
  // or no longer attached to a tab.
  virtual tabs::TabInterface* GetHostTab() const = 0;

  // Returns the URL category metric for this target, or nullopt if unavailable.
  virtual std::optional<DictationUrlCategory> GetUrlCategory() const;

  // Creates a context fetcher for this target, or returns nullptr if page
  // context should not be fetched. The returned fetcher borrows `this` and
  // must be destroyed before `this`. Defaults to nullptr.
  virtual std::unique_ptr<DictationContextFetcher> CreateContextFetcher();

  // Sets the text that was selected in the target when dictation started, for
  // potential transcription formatting adjustments if relevant for the target.
  virtual void SetSelectedText(std::u16string selected_text);

  // Returns the DOM node ID backing this target, or nullopt for non-DOM
  // targets.
  virtual std::optional<content::GlobalDOMNodeId> GetGlobalDOMNodeId() const;

  // Sets (or clears, if nullptr) the delegate notified of target lifecycle and
  // user-input events.
  virtual void SetDelegate(Delegate* delegate);

  // Notifies the target whether a dictation stream is currently active on it
  // (e.g. controlling whether stream-scoped key interception such as Enter is
  // enabled).
  virtual void SetStreamActive(bool active);

  // Returns `this` if backed by a DOM element, otherwise nullptr.
  virtual DomTarget* AsDomTarget();
  virtual const DomTarget* AsDomTarget() const;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_TARGET_H_
