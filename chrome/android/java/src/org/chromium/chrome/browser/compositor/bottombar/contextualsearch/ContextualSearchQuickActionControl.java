// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.drawable.Drawable;
import android.provider.Browser;
import android.text.TextUtils;
import android.widget.ImageView;

import androidx.core.graphics.drawable.DrawableCompat;

import org.chromium.base.IntentUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchUma;
import org.chromium.chrome.browser.contextualsearch.QuickActionCategory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;
import org.chromium.ui.resources.dynamics.ViewResourceInflater;
import org.chromium.ui.util.ColorUtils;

import java.net.URISyntaxException;
import java.util.List;

/**
 * Stores information related to a Contextual Search "quick action."
 * Actions can be activated through a tap on the Bar and include intents like calling a phone
 * number or launching Maps for a street address.
 */
public class ContextualSearchQuickActionControl extends ViewResourceInflater {
    private Context mContext;
    private String mQuickActionUri;
    private int mQuickActionCategory;
    private int mToolbarBackgroundColor;
    private boolean mHasQuickAction;
    private boolean mOpenQuickActionInChrome;
    private Intent mIntent;
    private String mCaption;

    /**
     * @param context The Android Context used to inflate the View.
     * @param resourceLoader The resource loader that will handle the snapshot capturing.
     */
    public ContextualSearchQuickActionControl(
            Context context, DynamicResourceLoader resourceLoader) {
        super(
                R.layout.contextual_search_quick_action_icon_view,
                R.id.contextual_search_quick_action_icon_view,
                context,
                null,
                resourceLoader);
        mContext = context;
    }

    /**
     * Gets the resource ID of the icon for the given category.
     * @param category Which application category the icon should be for.
     * @return         The resource ID or {@code null} if an unsupported category is supplied.
     */
    private static Integer getIconResId(@QuickActionCategory int category) {
        switch (category) {
            case QuickActionCategory.ADDRESS:
                return R.drawable.ic_place_googblue_36dp;
            case QuickActionCategory.EMAIL:
                return R.drawable.ic_email_googblue_36dp;
            case QuickActionCategory.EVENT:
                return R.drawable.ic_event_googblue_36dp;
            case QuickActionCategory.PHONE:
                return R.drawable.ic_phone_googblue_36dp;
            case QuickActionCategory.WEBSITE:
                return R.drawable.ic_link_grey600_36dp;
            default:
                return null;
        }
    }

    /**
     * Gets the caption string to show for the default app for the given category.
     * @param  category Which application category the string should be for.
     * @return          A string ID or {@code null} if an unsupported category is supplied.
     */
    private static Integer getDefaultAppCaptionId(@QuickActionCategory int category) {
        switch (category) {
            case QuickActionCategory.ADDRESS:
                return R.string.contextual_search_quick_action_caption_open;
            case QuickActionCategory.EMAIL:
                return R.string.contextual_search_quick_action_caption_email;
            case QuickActionCategory.EVENT:
                return R.string.contextual_search_quick_action_caption_event;
            case QuickActionCategory.PHONE:
                return R.string.contextual_search_quick_action_caption_phone;
            case QuickActionCategory.WEBSITE:
                return R.string.contextual_search_quick_action_caption_open;
            default:
                return null;
        }
    }

    /**
     * Gets the caption string to show for a generic app of the given category.
     * @param  category Which application category the string should be for.
     * @return          A string ID or {@code null} if an unsupported category is supplied.
     */
    private static Integer getFallbackCaptionId(@QuickActionCategory int category) {
        switch (category) {
            case QuickActionCategory.ADDRESS:
                return R.string.contextual_search_quick_action_caption_generic_map;
            case QuickActionCategory.EMAIL:
                return R.string.contextual_search_quick_action_caption_generic_email;
            case QuickActionCategory.EVENT:
                return R.string.contextual_search_quick_action_caption_generic_event;
            case QuickActionCategory.PHONE:
                return R.string.contextual_search_quick_action_caption_phone;
            case QuickActionCategory.WEBSITE:
                return R.string.contextual_search_quick_action_caption_generic_website;
            default:
                return null;
        }
    }

    /**
     * @param quickActionUri         The URI for the intent associated with the quick action.
     *                               If the URI is the empty string or cannot be parsed no quick
     *                               action will be available.
     * @param quickActionCategory    The {@link QuickActionCategory} for the quick action.
     * @param toolbarBackgroundColor The current toolbar background color. This may be
     *                               used for icon tinting.
     */
    public void setQuickAction(
            String quickActionUri, int quickActionCategory, int toolbarBackgroundColor) {
        if (TextUtils.isEmpty(quickActionUri)
                || quickActionCategory == QuickActionCategory.NONE
                || quickActionCategory >= QuickActionCategory.BOUNDARY) {
            reset();
            return;
        }

        mQuickActionUri = quickActionUri;
        mQuickActionCategory = quickActionCategory;
        mToolbarBackgroundColor = toolbarBackgroundColor;

        resolveIntent();
    }

    /**
     * Sends the intent associated with the quick action if one is available.
     * @param tab The current tab, used to load a URL if the quick action should open
     *            inside Chrome.
     */
    public void sendIntent(Tab tab) {
        if (mOpenQuickActionInChrome) {
            tab.loadUrl(new LoadUrlParams(mQuickActionUri));
            return;
        }

        if (mIntent == null) return;

        // Set the Browser application ID to us in case the user chooses Chrome
        // as the app from the intent picker.
        Context context = getContext();
        mIntent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());

        mIntent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        mIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        if (context instanceof ChromeTabbedActivity2) {
            // Set the window ID so the new tab opens in the correct window.
            mIntent.putExtra(IntentHandler.EXTRA_WINDOW_ID, 2);
        }

        IntentUtils.safeStartActivity(mContext, mIntent);
    }

    /**
     * @return The caption associated with the quick action or null if no quick action
     *         is available.
     */
    public String getCaption() {
        return mCaption;
    }

    /**
     * @return The resource id for the icon associated with the quick action or 0 if no
     *         quick action is available.
     */
    public int getIconResId() {
        return mHasQuickAction ? getViewId() : 0;
    }

    /**
     * @return Whether there is currently a quick action available.
     */
    public boolean hasQuickAction() {
        return mHasQuickAction;
    }

    /** Resets quick action data. */
    public void reset() {
        mQuickActionUri = "";
        mQuickActionCategory = QuickActionCategory.NONE;
        mHasQuickAction = false;
        mOpenQuickActionInChrome = false;
        mIntent = null;
        mCaption = "";
        mToolbarBackgroundColor = 0;
    }

    @Override
    protected boolean shouldAttachView() {
        return false;
    }

    private void resolveIntent() {
        try {
            mIntent = Intent.parseUri(mQuickActionUri, 0);
        } catch (URISyntaxException e) {
            // If the intent cannot be parsed, there is no quick action available.
            ContextualSearchUma.logQuickActionIntentResolution(mQuickActionCategory, 0);
            reset();
            return;
        }

        PackageManager packageManager = mContext.getPackageManager();

        // If a default is set, PackageManager#resolveActivity() will return the
        // ResolveInfo for the default activity.
        ResolveInfo possibleDefaultActivity = PackageManagerUtils.resolveActivity(mIntent, 0);

        // PackageManager#queryIntentActivities() will return a list of activities that
        // can handle the intent, sorted from best to worst. If there are no matching
        // activities, an empty list is returned.
        List<ResolveInfo> resolveInfoList = PackageManagerUtils.queryIntentActivities(mIntent, 0);

        int numMatchingActivities = 0;
        ResolveInfo defaultActivityResolveInfo = null;
        for (ResolveInfo resolveInfo : resolveInfoList) {
            if (resolveInfo.activityInfo != null && resolveInfo.activityInfo.exported) {
                numMatchingActivities++;
                if (possibleDefaultActivity == null
                        || possibleDefaultActivity.activityInfo == null) {
                    continue;
                }

                // Return early if this resolveInfo matches the possibleDefaultActivity.
                ActivityInfo possibleDefaultActivityInfo = possibleDefaultActivity.activityInfo;
                ActivityInfo resolveActivityInfo = resolveInfo.activityInfo;
                boolean matchesPossibleDefaultActivity =
                        TextUtils.equals(resolveActivityInfo.name, possibleDefaultActivityInfo.name)
                                && TextUtils.equals(
                                        resolveActivityInfo.packageName,
                                        possibleDefaultActivityInfo.packageName);

                if (matchesPossibleDefaultActivity) {
                    defaultActivityResolveInfo = resolveInfo;
                    break;
                }
            }
        }

        ContextualSearchUma.logQuickActionIntentResolution(
                mQuickActionCategory, numMatchingActivities);

        if (numMatchingActivities == 0) {
            reset();
            return;
        }

        mHasQuickAction = true;
        Drawable iconDrawable = null;
        int iconResId = 0;
        if (defaultActivityResolveInfo != null) {
            iconDrawable = defaultActivityResolveInfo.loadIcon(mContext.getPackageManager());

            if (mQuickActionCategory != QuickActionCategory.PHONE) {
                // Use the default app's name to construct the caption.
                mCaption =
                        mContext.getResources()
                                .getString(
                                        getDefaultAppCaptionId(mQuickActionCategory),
                                        defaultActivityResolveInfo.loadLabel(packageManager));
            } else {
                // The caption for phone numbers does not use the app's name.
                mCaption =
                        mContext.getResources()
                                .getString(getDefaultAppCaptionId(mQuickActionCategory));
            }
        } else if (mQuickActionCategory == QuickActionCategory.WEBSITE) {
            // If there is not a default app handler for a URL, open the quick action
            // inside of Chrome.
            mOpenQuickActionInChrome = true;

            if (mContext instanceof ChromeTabbedActivity) {
                // Use the app icon if this is a ChromeTabbedActivity instance.
                iconResId = R.mipmap.app_icon;
            } else {
                // Otherwise use the link icon.
                iconResId = getIconResId(mQuickActionCategory);

                if (mToolbarBackgroundColor != 0
                        && !ThemeUtils.isUsingDefaultToolbarColor(
                                mContext, false, mToolbarBackgroundColor)
                        && ColorUtils.shouldUseLightForegroundOnBackground(
                                mToolbarBackgroundColor)) {
                    // Tint the link icon to match the custom tab toolbar.
                    iconDrawable = mContext.getDrawable(iconResId);
                    iconDrawable.mutate();
                    DrawableCompat.setTint(iconDrawable, mToolbarBackgroundColor);
                }
            }
            mCaption =
                    mContext.getResources().getString(getFallbackCaptionId(mQuickActionCategory));
        } else {
            iconResId = getIconResId(mQuickActionCategory);
            mCaption =
                    mContext.getResources().getString(getFallbackCaptionId(mQuickActionCategory));
        }

        inflate();

        if (iconDrawable != null) {
            ((ImageView) getView()).setImageDrawable(iconDrawable);
        } else {
            ((ImageView) getView()).setImageResource(iconResId);
        }

        invalidate();
    }
}
