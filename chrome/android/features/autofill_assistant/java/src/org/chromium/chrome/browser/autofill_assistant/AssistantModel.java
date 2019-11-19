// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantCarouselModel;
import org.chromium.chrome.browser.autofill_assistant.details.AssistantDetailsModel;
import org.chromium.chrome.browser.autofill_assistant.form.AssistantFormModel;
import org.chromium.chrome.browser.autofill_assistant.header.AssistantHeaderModel;
import org.chromium.chrome.browser.autofill_assistant.infobox.AssistantInfoBoxModel;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayModel;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantCollectUserDataModel;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * State for the Autofill Assistant UI.
 */
@JNINamespace("autofill_assistant")
class AssistantModel extends PropertyModel {
    static final WritableBooleanPropertyKey ALLOW_SOFT_KEYBOARD = new WritableBooleanPropertyKey();
    static final WritableBooleanPropertyKey ALLOW_TALKBACK_ON_WEBSITE =
            new WritableBooleanPropertyKey();
    static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey();

    /** The web contents the Autofill Assistant is associated with. */
    static final WritableObjectPropertyKey<WebContents> WEB_CONTENTS =
            new WritableObjectPropertyKey<>();

    private final AssistantOverlayModel mOverlayModel;
    private final AssistantHeaderModel mHeaderModel = new AssistantHeaderModel();
    private final AssistantDetailsModel mDetailsModel = new AssistantDetailsModel();
    private final AssistantInfoBoxModel mInfoBoxModel = new AssistantInfoBoxModel();
    private final AssistantCollectUserDataModel mCollectUserDataModel =
            new AssistantCollectUserDataModel();
    private final AssistantFormModel mFormModel = new AssistantFormModel();
    private final AssistantCarouselModel mSuggestionsModel = new AssistantCarouselModel();
    private final AssistantCarouselModel mActionsModel = new AssistantCarouselModel();

    AssistantModel() {
        this(new AssistantOverlayModel());
    }

    AssistantModel(AssistantOverlayModel overlayModel) {
        super(ALLOW_SOFT_KEYBOARD, VISIBLE, WEB_CONTENTS, ALLOW_TALKBACK_ON_WEBSITE);
        mOverlayModel = overlayModel;
    }

    @CalledByNative
    public AssistantOverlayModel getOverlayModel() {
        return mOverlayModel;
    }

    @CalledByNative
    public AssistantHeaderModel getHeaderModel() {
        return mHeaderModel;
    }

    @CalledByNative
    public AssistantDetailsModel getDetailsModel() {
        return mDetailsModel;
    }

    @CalledByNative
    public AssistantInfoBoxModel getInfoBoxModel() {
        return mInfoBoxModel;
    }

    @CalledByNative
    public AssistantCollectUserDataModel getCollectUserDataModel() {
        return mCollectUserDataModel;
    }

    @CalledByNative
    public AssistantFormModel getFormModel() {
        return mFormModel;
    }

    public AssistantCarouselModel getSuggestionsModel() {
        return mSuggestionsModel;
    }

    public AssistantCarouselModel getActionsModel() {
        return mActionsModel;
    }

    @CalledByNative
    private void setAllowSoftKeyboard(boolean allowed) {
        set(ALLOW_SOFT_KEYBOARD, allowed);
    }

    @CalledByNative
    private void setAllowTalkbackOnWebsite(boolean allowed) {
        set(ALLOW_TALKBACK_ON_WEBSITE, allowed);
    }

    @CalledByNative
    void setVisible(boolean visible) {
        set(VISIBLE, visible);
    }

    @CalledByNative
    private boolean getVisible() {
        return get(VISIBLE);
    }

    @CalledByNative
    private void setWebContents(WebContents contents) {
        set(WEB_CONTENTS, contents);
    }
}
