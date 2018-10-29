// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.support.annotation.ColorRes;
import android.support.annotation.DrawableRes;
import android.support.annotation.StringRes;
import android.support.v7.widget.AppCompatTextView;
import android.text.Layout;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.TintedDrawable;

import java.util.ArrayList;
import java.util.List;

/**
 * Represents the view inside the page info popup.
 */
public class PageInfoView extends FrameLayout implements OnClickListener, OnLongClickListener {
    /**
     * A TextView which truncates and displays a URL such that the origin is always visible.
     * The URL can be expanded by clicking on the it.
     */
    public static class ElidedUrlTextView extends AppCompatTextView {
        // The number of lines to display when the URL is truncated. This number
        // should still allow the origin to be displayed. NULL before
        // setUrlAfterLayout() is called.
        private Integer mTruncatedUrlLinesToDisplay;

        // The number of lines to display when the URL is expanded. This should be enough to display
        // at most two lines of the fragment if there is one in the URL.
        private Integer mFullLinesToDisplay;

        // If true, the text view will show the truncated text. If false, it
        // will show the full, expanded text.
        private boolean mIsShowingTruncatedText = true;

        // The length of the URL's origin in number of characters.
        private int mOriginLength = -1;

        // The maximum number of lines currently shown in the view
        private int mCurrentMaxLines = Integer.MAX_VALUE;

        /** Constructor for inflating from XML. */
        public ElidedUrlTextView(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        @Override
        public void setMaxLines(int maxlines) {
            super.setMaxLines(maxlines);
            mCurrentMaxLines = maxlines;
        }

        /**
         * Find the number of lines of text which must be shown in order to display the character at
         * a given index.
         */
        private int getLineForIndex(int index) {
            Layout layout = getLayout();
            int endLine = 0;
            while (endLine < layout.getLineCount() && layout.getLineEnd(endLine) < index) {
                endLine++;
            }
            // Since endLine is an index, add 1 to get the number of lines.
            return endLine + 1;
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            setMaxLines(Integer.MAX_VALUE);
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            assert mOriginLength >= 0 : "setUrl() must be called before layout.";
            String urlText = getText().toString();

            // Find the range of lines containing the origin.
            int originEndLine = getLineForIndex(mOriginLength);

            // Display an extra line so we don't accidentally hide the origin with
            // ellipses
            mTruncatedUrlLinesToDisplay = originEndLine + 1;

            // Find the line where the fragment starts. Since # is a reserved character, it is safe
            // to just search for the first # to appear in the url.
            int fragmentStartIndex = urlText.indexOf('#');
            if (fragmentStartIndex == -1) fragmentStartIndex = urlText.length();

            int fragmentStartLine = getLineForIndex(fragmentStartIndex);
            mFullLinesToDisplay = fragmentStartLine + 1;

            // If there is no origin (according to OmniboxUrlEmphasizer), make sure the fragment is
            // still hidden correctly.
            if (mFullLinesToDisplay < mTruncatedUrlLinesToDisplay) {
                mTruncatedUrlLinesToDisplay = mFullLinesToDisplay;
            }

            if (updateMaxLines()) super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }

        /**
         * Sets the URL and the length of the URL's origin.
         * Must be called before layout.
         *
         * @param url The URL.
         * @param originLength The length of the URL's origin in number of characters.
         */
        public void setUrl(CharSequence url, int originLength) {
            assert originLength >= 0 && originLength <= url.length();
            setText(url);
            mOriginLength = originLength;
        }

        /**
         * Toggles truncating/expanding the URL text. If the URL text is not
         * truncated, has no effect.
         */
        public void toggleTruncation() {
            mIsShowingTruncatedText = !mIsShowingTruncatedText;
            updateMaxLines();
        }

        private boolean updateMaxLines() {
            int maxLines = mFullLinesToDisplay;
            if (mIsShowingTruncatedText) {
                maxLines = mTruncatedUrlLinesToDisplay;
            }
            if (maxLines != mCurrentMaxLines) {
                setMaxLines(maxLines);
                return true;
            }
            return false;
        }
    }

    /**  Parameters to configure the view of the page info popup. */
    public static class PageInfoViewParams {
        public boolean urlTitleShown = true;
        public boolean connectionMessageShown = true;
        public boolean instantAppButtonShown = true;
        public boolean siteSettingsButtonShown = true;
        public boolean openOnlineButtonShown = true;
        public boolean previewUIShown = true;
        public boolean separatorShown = true;

        public Runnable urlTitleClickCallback;
        public Runnable urlTitleLongClickCallback;
        public Runnable instantAppButtonClickCallback;
        public Runnable siteSettingsButtonClickCallback;
        public Runnable openOnlineButtonClickCallback;
        public Runnable previewShowOriginalClickCallback;

        public CharSequence url;
        public CharSequence previewLoadOriginalMessage;
        public CharSequence previewStaleTimestamp;
        public int urlOriginLength;
    }

    /**  Parameters to configure the view of a permission row. */
    public static class PermissionParams {
        public CharSequence status;
        public @DrawableRes int iconResource;
        public @ColorRes int iconTintColorResource;
        public @StringRes int warningTextResource;
        public @StringRes int subtitleTextResource;
        public Runnable clickCallback;
    }

    /**  Parameters to configure the view of the connection message. */
    public static class ConnectionInfoParams {
        public CharSequence message;
        public CharSequence summary;
        public Runnable clickCallback;
    }

    private static final int FADE_DURATION_MS = 200;
    private static final int FADE_IN_BASE_DELAY_MS = 150;
    private static final int FADE_IN_DELAY_OFFSET_MS = 20;

    private final ElidedUrlTextView mUrlTitle;
    private final TextView mConnectionSummary;
    private final TextView mConnectionMessage;
    private final TextView mPreviewMessage;
    private final TextView mPreviewStaleTimestamp;
    private final TextView mPreviewLoadOriginal;
    private final LinearLayout mPermissionsList;
    private final View mSeparator;
    private final Button mInstantAppButton;
    private final Button mSiteSettingsButton;
    private final Button mOpenOnlineButton;
    private final Runnable mUrlTitleLongClickCallback;

    public PageInfoView(Context context, PageInfoViewParams params) {
        super(context);

        // Find the container and all it's important subviews.
        LayoutInflater.from(context).inflate(R.layout.page_info, this, true);
        mUrlTitle = (ElidedUrlTextView) findViewById(R.id.page_info_url);
        mConnectionSummary = (TextView) findViewById(R.id.page_info_connection_summary);
        mConnectionMessage = (TextView) findViewById(R.id.page_info_connection_message);
        mPreviewMessage = (TextView) findViewById(R.id.page_info_preview_message);
        mPreviewStaleTimestamp = (TextView) findViewById(R.id.page_info_stale_preview_timestamp);
        mPreviewLoadOriginal = (TextView) findViewById(R.id.page_info_preview_load_original);
        mPermissionsList = (LinearLayout) findViewById(R.id.page_info_permissions_list);
        mSeparator = (View) findViewById(R.id.page_info_separator);
        mInstantAppButton = (Button) findViewById(R.id.page_info_instant_app_button);
        mSiteSettingsButton = (Button) findViewById(R.id.page_info_site_settings_button);
        mOpenOnlineButton = (Button) findViewById(R.id.page_info_open_online_button);

        mUrlTitle.setUrl(params.url, params.urlOriginLength);
        mUrlTitleLongClickCallback = params.urlTitleLongClickCallback;
        if (params.urlTitleLongClickCallback != null) {
            mUrlTitle.setOnLongClickListener(this);
        }

        initializePageInfoViewChild(
                mUrlTitle, params.urlTitleShown, 0f, params.urlTitleClickCallback);
        // Hide the summary until its text is set.
        initializePageInfoViewChild(mConnectionSummary, false, 0f, null);
        initializePageInfoViewChild(mConnectionMessage, params.connectionMessageShown, 0f, null);
        // Hide the permissions list for sites with no permissions.
        initializePageInfoViewChild(mPermissionsList, false, 1f, null);
        initializePageInfoViewChild(mInstantAppButton, params.instantAppButtonShown, 0f,
                params.instantAppButtonClickCallback);
        initializePageInfoViewChild(mSiteSettingsButton, params.siteSettingsButtonShown, 0f,
                params.siteSettingsButtonClickCallback);
        // The open online button should not fade in.
        initializePageInfoViewChild(mOpenOnlineButton, params.openOnlineButtonShown, 1f,
                params.openOnlineButtonClickCallback);
        // Previews UI initialization.
        initializePageInfoViewChild(mPreviewMessage, params.previewUIShown, 0f, null);
        initializePageInfoViewChild(mPreviewLoadOriginal, params.previewUIShown, 0f,
                params.previewShowOriginalClickCallback);
        initializePageInfoViewChild(mPreviewStaleTimestamp,
                params.previewUIShown && !TextUtils.isEmpty(params.previewStaleTimestamp), 0f,
                null);
        initializePageInfoViewChild(mSeparator, params.separatorShown, 0f, null);
        mPreviewLoadOriginal.setText(params.previewLoadOriginalMessage);
        if (!TextUtils.isEmpty(params.previewStaleTimestamp)) {
            mPreviewStaleTimestamp.setText(params.previewStaleTimestamp);
        }
    }

    public void setPermissions(List<PermissionParams> permissionParamsList) {
        mPermissionsList.removeAllViews();
        // If we have at least one permission show the lower permissions area.
        mPermissionsList.setVisibility(!permissionParamsList.isEmpty() ? View.VISIBLE : View.GONE);
        for (PermissionParams params : permissionParamsList) {
            mPermissionsList.addView(createPermissionRow(params));
        }
    }

    public void setConnectionInfo(ConnectionInfoParams params) {
        if (params.summary != null) {
            mConnectionSummary.setVisibility(View.VISIBLE);
            mConnectionSummary.setText(params.summary);
        }
        if (params.message != null) {
            mConnectionMessage.setVisibility(View.VISIBLE);
            mConnectionMessage.setText(params.message);
            if (params.clickCallback != null) {
                mConnectionMessage.setTag(R.id.page_info_click_callback, params.clickCallback);
                mConnectionMessage.setOnClickListener(this);
            }
        }
    }

    public Animator createEnterExitAnimation(boolean isEnter) {
        return createFadeAnimations(isEnter);
    }

    public void toggleUrlTruncation() {
        mUrlTitle.toggleTruncation();
    }

    public void disableInstantAppButton() {
        mInstantAppButton.setEnabled(false);
    }

    @Override
    public void onClick(View view) {
        Object clickCallbackObj = view.getTag(R.id.page_info_click_callback);
        if (!(clickCallbackObj instanceof Runnable)) {
            throw new IllegalStateException("Unable to find click callback for view: " + view);
        }
        Runnable clickCallback = (Runnable) clickCallbackObj;
        clickCallback.run();
    }

    @Override
    public boolean onLongClick(View view) {
        assert view == mUrlTitle;
        assert mUrlTitleLongClickCallback != null;
        mUrlTitleLongClickCallback.run();
        return true;
    }

    private void initializePageInfoViewChild(
            View child, boolean shown, float alpha, Runnable clickCallback) {
        // Make all subviews transparent until the page info view is faded in.
        child.setAlpha(alpha);
        child.setVisibility(shown ? View.VISIBLE : View.GONE);
        child.setTag(R.id.page_info_click_callback, clickCallback);
        if (clickCallback == null) return;
        child.setOnClickListener(this);
    }

    private View createPermissionRow(PermissionParams params) {
        View permissionRow =
                LayoutInflater.from(getContext()).inflate(R.layout.page_info_permission_row, null);

        TextView permissionStatus =
                (TextView) permissionRow.findViewById(R.id.page_info_permission_status);
        permissionStatus.setText(params.status);

        ImageView permissionIcon =
                (ImageView) permissionRow.findViewById(R.id.page_info_permission_icon);
        if (params.iconTintColorResource == 0) {
            permissionIcon.setImageDrawable(
                    TintedDrawable.constructTintedDrawable(getContext(), params.iconResource));
        } else {
            permissionIcon.setImageResource(params.iconResource);
            permissionIcon.setColorFilter(ApiCompatibilityUtils.getColor(
                    getContext().getResources(), params.iconTintColorResource));
        }

        if (params.warningTextResource != 0) {
            TextView permissionUnavailable = (TextView) permissionRow.findViewById(
                    R.id.page_info_permission_unavailable_message);
            permissionUnavailable.setVisibility(View.VISIBLE);
            permissionUnavailable.setText(params.warningTextResource);
        }

        if (params.subtitleTextResource != 0) {
            TextView permissionSubtitle =
                    (TextView) permissionRow.findViewById(R.id.page_info_permission_subtitle);
            permissionSubtitle.setVisibility(View.VISIBLE);
            permissionSubtitle.setText(params.subtitleTextResource);
        }

        if (params.clickCallback != null) {
            permissionRow.setTag(R.id.page_info_click_callback, params.clickCallback);
            permissionRow.setOnClickListener(this);
        }

        return permissionRow;
    }

    /**
     * Create a list of all the views which we want to individually fade in.
     */
    private List<View> collectAnimatableViews() {
        List<View> animatableViews = new ArrayList<View>();
        animatableViews.add(mUrlTitle);
        animatableViews.add(mConnectionSummary);
        animatableViews.add(mConnectionMessage);
        animatableViews.add(mPreviewMessage);
        animatableViews.add(mPreviewStaleTimestamp);
        animatableViews.add(mPreviewLoadOriginal);
        animatableViews.add(mSeparator);
        animatableViews.add(mInstantAppButton);
        for (int i = 0; i < mPermissionsList.getChildCount(); i++) {
            animatableViews.add(mPermissionsList.getChildAt(i));
        }
        animatableViews.add(mSiteSettingsButton);

        return animatableViews;
    }

    /**
     * Create an animator to fade an individual dialog element.
     */
    private Animator createInnerFadeAnimation(final View view, int position, boolean isEnter) {
        ObjectAnimator alphaAnim;

        if (isEnter) {
            view.setAlpha(0f);
            alphaAnim = ObjectAnimator.ofFloat(view, View.ALPHA, 1f);
            alphaAnim.setStartDelay(FADE_IN_BASE_DELAY_MS + FADE_IN_DELAY_OFFSET_MS * position);
        } else {
            alphaAnim = ObjectAnimator.ofFloat(view, View.ALPHA, 0f);
        }

        alphaAnim.setDuration(FADE_DURATION_MS);
        return alphaAnim;
    }

    /**
     * Create animations for fading the view in/out.
     */
    private Animator createFadeAnimations(boolean isEnter) {
        AnimatorSet animation = new AnimatorSet();
        AnimatorSet.Builder builder = animation.play(new AnimatorSet());

        List<View> animatableViews = collectAnimatableViews();
        for (int i = 0; i < animatableViews.size(); i++) {
            View view = animatableViews.get(i);
            if (view.getVisibility() == View.VISIBLE) {
                Animator anim = createInnerFadeAnimation(view, i, isEnter);
                builder.with(anim);
            }
        }

        return animation;
    }

    @VisibleForTesting
    public String getUrlTitleForTesting() {
        return mUrlTitle.getText().toString();
    }
}
