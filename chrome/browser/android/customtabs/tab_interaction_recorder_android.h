// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CUSTOMTABS_TAB_INTERACTION_RECORDER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_CUSTOMTABS_TAB_INTERACTION_RECORDER_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace customtabs {

// Autofill observer impl for the TabInteractionRecorderAndroid to use.
class AutofillObserverImpl : public autofill::AutofillManager::Observer {
 public:
  using OnFormInteractionCallback = base::OnceCallback<void()>;

  explicit AutofillObserverImpl(
      autofill::AutofillManager* autofill_manager,
      OnFormInteractionCallback form_interaction_callback);

  AutofillObserverImpl(const AutofillObserverImpl&) = delete;
  AutofillObserverImpl operator=(const AutofillObserverImpl&) = delete;
  ~AutofillObserverImpl() override;

  // AutofillManager::Observer:
  void OnFormSubmitted() override;
  void OnSelectControlDidChange() override;
  void OnTextFieldDidChange() override;
  void OnTextFieldDidScroll() override;

 private:
  void OnFormInteraction();
  void Invalidate();

  raw_ptr<autofill::AutofillManager, DanglingUntriaged> autofill_manager_;
  OnFormInteractionCallback form_interaction_callback_;
};

// Class that record interaction from the web contents. The definition
// of an "interaction" includes user's engagement with text inputs or selection
// inputs, or changes in navigation stacks.
//
// To attach this class to the web contents correctly, it has to be setup
// before the first navigation finishes in order to attach observers to
// corresponding autofill managers; otherwise the interaction with first frame
// would be missing.
class TabInteractionRecorderAndroid
    : public content::WebContentsObserver,
      public content::WebContentsUserData<TabInteractionRecorderAndroid> {
 public:
  ~TabInteractionRecorderAndroid() override;

  // Return whether the |web_contents()| has navigated away from the first page.
  bool HasNavigatedFromFirstPage() const;

  // Return whether the |web_contents()| has seen any form interactions.
  bool has_form_interactions() const { return has_form_interactions_; }
  bool did_get_user_interaction() const { return did_get_user_interaction_; }

  // content::WebContentsObserver:

  // Dispatch an AutofillManagerObserver when a frame becomes active, or remove
  // the old AutofillManagerObserver when a frame becomes inactive.
  void RenderFrameHostStateChanged(
      content::RenderFrameHost* render_frame_host,
      content::RenderFrameHost::LifecycleState old_state,
      content::RenderFrameHost::LifecycleState new_state) override;
  // When a new frame is created, |RenderFrameHostStateChanged| happen earlier
  // than a AutofillManager initialized in such frame, which happened at
  // |DidFinishNavigation|. Observing |DidFinishNavigation| ensure this class
  // to dispatch the AutofillManagerObserver for newly created AutofillManager.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidGetUserInteraction(const blink::WebInputEvent& event) override;

  // JNI methods
  jboolean DidGetUserInteraction(JNIEnv* env) const;
  jboolean HadInteraction(JNIEnv* env) const;
  void Reset(JNIEnv* env);

#ifdef UNIT_TEST
  void SetAutofillManagerForTest(
      autofill::AutofillManager* test_autofill_manager) {
    test_autofill_manager_ = test_autofill_manager;
  }
#endif

 private:
  explicit TabInteractionRecorderAndroid(content::WebContents* web_contents);

  friend class AutofillObserverImpl;
  void StartObservingFrame(content::RenderFrameHost* render_frame_host);
  void SetHasFormInteractions();

  bool did_get_user_interaction_ = false;
  bool has_form_interactions_ = false;
  std::unordered_map<content::GlobalRenderFrameHostId,
                     std::unique_ptr<AutofillObserverImpl>,
                     content::GlobalRenderFrameHostIdHasher>
      rfh_observer_map_;
  raw_ptr<autofill::AutofillManager> test_autofill_manager_ = nullptr;

  // content::WebContentsUserData<TabInteractionRecorderAndroid>
  friend class content::WebContentsUserData<TabInteractionRecorderAndroid>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<TabInteractionRecorderAndroid> weak_factory_{this};
};

}  // namespace customtabs

#endif  // CHROME_BROWSER_ANDROID_CUSTOMTABS_TAB_INTERACTION_RECORDER_ANDROID_H_
