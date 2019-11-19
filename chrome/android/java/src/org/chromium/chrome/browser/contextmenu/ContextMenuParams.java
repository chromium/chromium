// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.text.TextUtils;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.blink_public.common.ContextMenuDataMediaType;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.base.MenuSourceType;
/**
 * A list of parameters that explain what kind of context menu to show the user.  This data is
 * generated from content/public/common/context_menu_params.h.
 */
@JNINamespace("ContextMenuParamsAndroid")
public class ContextMenuParams {
    private final String mPageUrl;
    private final String mLinkUrl;
    private final String mLinkText;
    private final String mTitleText;
    private final String mUnfilteredLinkUrl;
    private final String mSrcUrl;
    private final Referrer mReferrer;

    private final boolean mIsAnchor;
    private final boolean mIsImage;
    private final boolean mIsVideo;
    private final boolean mCanSaveMedia;

    private final int mTriggeringTouchXDp;
    private final int mTriggeringTouchYDp;

    private final int mSourceType;

    /**
     * @return The URL associated with the main frame of the page that triggered the context menu.
     */
    public String getPageUrl() {
        return mPageUrl;
    }

    /**
     * @return The link URL, if any.
     */
    public String getLinkUrl() {
        return mLinkUrl;
    }

    /**
     * @return The link text, if any.
     */
    public String getLinkText() {
        return mLinkText;
    }

    /**
     * @return The title or alt attribute (if title is not available).
     */
    public String getTitleText() {
        return mTitleText;
    }

    /**
     * @return The unfiltered link URL, if any.
     */
    public String getUnfilteredLinkUrl() {
        return mUnfilteredLinkUrl;
    }

    /**
     * @return The source URL.
     */
    public String getSrcUrl() {
        return mSrcUrl;
    }

    /**
     * @return the referrer associated with the frame on which the menu is invoked
     */
    public Referrer getReferrer() {
        return mReferrer;
    }

    /**
     * @return Whether or not the context menu is being shown for an anchor.
     */
    public boolean isAnchor() {
        return mIsAnchor;
    }

    /**
     * @return Whether or not the context menu is being shown for an image.
     */
    public boolean isImage() {
        return mIsImage;
    }

    /**
     * @return Whether or not the context menu is being shown for a video.
     */
    public boolean isVideo() {
        return mIsVideo;
    }

    public boolean canSaveMedia() {
        return mCanSaveMedia;
    }

    /**
     * @return Whether or not the context menu is been shown for a download item.
     */
    public boolean isFile() {
        if (!TextUtils.isEmpty(mSrcUrl) && mSrcUrl.startsWith(UrlConstants.FILE_URL_PREFIX)) {
            return true;
        }
        return false;
    }

    /**
     * @return The x-coordinate of the touch that triggered the context menu in dp relative to the
     *         render view; 0 corresponds to the left edge.
     */
    public int getTriggeringTouchXDp() {
        return mTriggeringTouchXDp;
    }

    /**
     * @return The y-coordinate of the touch that triggered the context menu in dp relative to the
     *         render view; 0 corresponds to the left edge.
     */
    public int getTriggeringTouchYDp() {
        return mTriggeringTouchYDp;
    }

    /**
     * @return The method used to cause the context menu to be shown. For example, right mouse click
     *         or long press.
     */
    public int getSourceType() {
        return mSourceType;
    }

    /**
     * @return The valid url of a ContextMenuParams.
     */
    public String getUrl() {
        if (isAnchor() && !TextUtils.isEmpty(getLinkUrl())) {
            return getLinkUrl();
        } else {
            return getSrcUrl();
        }
    }

    public ContextMenuParams(@ContextMenuDataMediaType int mediaType, String pageUrl,
            String linkUrl, String linkText, String unfilteredLinkUrl, String srcUrl,
            String titleText, Referrer referrer, boolean canSaveMedia, int triggeringTouchXDp,
            int triggeringTouchYDp, @MenuSourceType int sourceType) {
        mPageUrl = pageUrl;
        mLinkUrl = linkUrl;
        mLinkText = linkText;
        mTitleText = titleText;
        mUnfilteredLinkUrl = unfilteredLinkUrl;
        mSrcUrl = srcUrl;
        mReferrer = referrer;

        mIsAnchor = !TextUtils.isEmpty(linkUrl);
        mIsImage = mediaType == ContextMenuDataMediaType.IMAGE;
        mIsVideo = mediaType == ContextMenuDataMediaType.VIDEO;
        mCanSaveMedia = canSaveMedia;
        mTriggeringTouchXDp = triggeringTouchXDp;
        mTriggeringTouchYDp = triggeringTouchYDp;
        mSourceType = sourceType;
    }

    @CalledByNative
    private static ContextMenuParams create(@ContextMenuDataMediaType int mediaType, String pageUrl,
            String linkUrl, String linkText, String unfilteredLinkUrl, String srcUrl,
            String titleText, String sanitizedReferrer, int referrerPolicy, boolean canSaveMedia,
            int triggeringTouchXDp, int triggeringTouchYDp, @MenuSourceType int sourceType) {
        Referrer referrer = TextUtils.isEmpty(sanitizedReferrer)
                ? null : new Referrer(sanitizedReferrer, referrerPolicy);
        return new ContextMenuParams(mediaType, pageUrl, linkUrl, linkText, unfilteredLinkUrl,
                srcUrl, titleText, referrer, canSaveMedia, triggeringTouchXDp, triggeringTouchYDp,
                sourceType);
    }
}
