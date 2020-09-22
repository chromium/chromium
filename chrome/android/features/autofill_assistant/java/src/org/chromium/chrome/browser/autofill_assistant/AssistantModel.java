// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantCarouselModel;
import org.chromium.chrome.browser.autofill_assistant.details.AssistantDetailsModel;
import org.chromium.chrome.browser.autofill_assistant.form.AssistantFormModel;
import org.chromium.chrome.browser.autofill_assistant.generic_ui.AssistantGenericUiModel;
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

    static final WritableObjectPropertyKey<AssistantBottomBarDelegate> BOTTOM_BAR_DELEGATE =
            new WritableObjectPropertyKey<>();
    static final WritableIntPropertyKey BOTTOM_SHEET_STATE = new WritableIntPropertyKey();
    static final WritableFloatPropertyKey TALKBACK_SHEET_SIZE_FRACTION =
            new WritableFloatPropertyKey();
    static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey();
    static final WritableBooleanPropertyKey PEEK_MODE_DISABLED = new WritableBooleanPropertyKey();

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
    private final AssistantCarouselModel mActionsModel = new AssistantCarouselModel();
    private final AssistantGenericUiModel mGenericUiModel = new AssistantGenericUiModel();

    AssistantModel() {
        this(new AssistantOverlayModel());
    }

    AssistantModel(AssistantOverlayModel overlayModel) {
        super(ALLOW_SOFT_KEYBOARD, ALLOW_TALKBACK_ON_WEBSITE, BOTTOM_BAR_DELEGATE,
                BOTTOM_SHEET_STATE, TALKBACK_SHEET_SIZE_FRACTION, VISIBLE, PEEK_MODE_DISABLED,
                WEB_CONTENTS);
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

    public AssistantCarouselModel getActionsModel() {
        return mActionsModel;
    }

    @CalledByNative
    public AssistantGenericUiModel getGenericUiModel() {
        return mGenericUiModel;
    }

    public AssistantBottomBarDelegate getBottomBarDelegate() {
        return get(BOTTOM_BAR_DELEGATE);
    }

    public int getBottomSheetState() {
        return get(BOTTOM_SHEET_STATE);
    }

    public void setBottomSheetState(int state) {
        set(BOTTOM_SHEET_STATE, state);
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
    private void setBottomBarDelegate(AssistantBottomBarDelegate delegate) {
        set(BOTTOM_BAR_DELEGATE, delegate);
    }

    @CalledByNative
    private void setTalkbackSheetSizeFraction(float fraction) {
        set(TALKBACK_SHEET_SIZE_FRACTION, fraction);
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
    private void setPeekModeDisabled(boolean disabled) {
        set(PEEK_MODE_DISABLED, disabled);
    }

    @CalledByNative
    private void setWebContents(WebContents contents) {
        set(WEB_CONTENTS, contents);
    }
}
