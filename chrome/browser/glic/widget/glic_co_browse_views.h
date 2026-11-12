// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_CO_BROWSE_VIEWS_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_CO_BROWSE_VIEWS_H_

#include <cstdint>
#include <memory>
#include <utility>

#include "base/android/scoped_java_ref.h"
#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/memory/stack_allocated.h"
#include "base/memory/weak_ptr.h"
#include "components/tabs/public/tab_interface.h"

namespace content {
class WebContents;
}  // namespace content

namespace context_sharing {
class CoBrowseViewsBridge;
}  // namespace context_sharing

class Profile;

namespace glic {

// Controller managing the CoBrowseViewsBridge and native-to-Java component
// providers for the Glic side panel / bottom sheet on Android. Owned by
// GlicSidePanelCoordinator and configured by the active or inactive side panel
// embedder.
class GlicCoBrowseViews {
 public:
  struct Params {
    content::WebContents* web_contents = nullptr;
    bool is_loading = false;
    base::RepeatingClosure close_callback;
    base::RepeatingClosure native_fre_accepted_callback;

    STACK_ALLOCATED();
  };

  // `profile` must be non-null and outlive `this`.
  GlicCoBrowseViews(base::WeakPtr<tabs::TabInterface> tab,
                    Profile* profile,
                    bool is_bottom_sheet);
  ~GlicCoBrowseViews();

  GlicCoBrowseViews(const GlicCoBrowseViews&) = delete;
  GlicCoBrowseViews& operator=(const GlicCoBrowseViews&) = delete;

  // Configures the view state and callbacks for the current embedder. Returns a
  // closure runner that resets callbacks, loading state, and WebContents if no
  // newer embedder has since called `Configure()`.
  [[nodiscard]] base::ScopedClosureRunner Configure(Params params);

  // Obtains or lazily creates the Java CoBrowseViews jobject.
  base::android::ScopedJavaLocalRef<jobject> GetOrCreateJavaCoBrowseViews();

  // Recreates the CoBrowseViews in Java (e.g. when WindowAndroid/layout is
  // ready).
  void RecreateJavaCoBrowseViews();

  // Resets native FRE when the panel closes.
  void OnPanelClosed();

  // Sets or updates the active guest WebContents.
  void SetWebContents(content::WebContents* web_contents);

  // Sets or updates whether the loading placeholder should show.
  void SetIsLoading(bool is_loading);

  // Prepares the view before showing the panel.
  void PrepareForShow(bool show_native_fre);

  // Called from JNI when the user accepts the native FRE.
  void OnNativeFreAccepted();

  // Testing helpers.
  content::WebContents* web_contents_for_testing() const {
    return web_contents_.get();
  }
  bool is_loading_for_testing() const { return is_loading_; }
  bool HasViewsBridgeForTesting() const { return views_bridge_ != nullptr; }
  bool HasJavaWebContentsForTesting() const;
  bool IsShowingNativeFreForTesting() const;
  bool ShouldShowLoadingPlaceholderForTesting() const;
  void RequestCloseForTesting();

  base::WeakPtr<GlicCoBrowseViews> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void EnsureBridgeCreated();
  void ResetIfGenerationMatches(uint64_t generation);
  void OnCloseRequested();
  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason detach_reason);
  bool ShouldShowNativeFre() const;
  void UpdateJavaNativeFre();
  void ResetNativeFre();
  bool ShouldShowLoadingPlaceholder() const;
  void UpdateJavaIsLoading();

  base::WeakPtr<tabs::TabInterface> tab_;
  const raw_ref<Profile> profile_;
  const bool is_bottom_sheet_;

  uint64_t generation_ = 0;
  base::WeakPtr<content::WebContents> web_contents_;
  bool is_loading_ = false;
  bool showing_native_fre_ = false;

  base::RepeatingClosure close_callback_;
  base::RepeatingClosure on_native_fre_accepted_;

  base::CallbackListSubscription will_detach_subscription_;

  std::unique_ptr<context_sharing::CoBrowseViewsBridge> views_bridge_;
  base::android::ScopedJavaGlobalRef<jobject> component_provider_;

  base::WeakPtrFactory<GlicCoBrowseViews> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_CO_BROWSE_VIEWS_H_
