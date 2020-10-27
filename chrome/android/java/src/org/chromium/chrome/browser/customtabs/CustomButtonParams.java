// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.view.ViewGroup;
import android.widget.ImageButton;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.TintedDrawable;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Container for all parameters related to creating a customizable button.
 */
public class CustomButtonParams {
    private static final String TAG = "CustomTabs";

    private final PendingIntent mPendingIntent;
    private int mId;
    private Bitmap mIcon;
    private String mDescription;
    private boolean mShouldTint;
    private boolean mIsOnToolbar;

    @VisibleForTesting
    static final String SHOW_ON_TOOLBAR = "android.support.customtabs.customaction.SHOW_ON_TOOLBAR";

    private CustomButtonParams(int id, Bitmap icon, String description,
            @Nullable PendingIntent pendingIntent, boolean tinted, boolean onToolbar) {
        mId = id;
        mIcon = icon;
        mDescription = description;
        mPendingIntent = pendingIntent;
        mShouldTint = tinted;
        mIsOnToolbar = onToolbar;
    }

    /**
     * Replaces the current icon and description with new ones.
     */
    void update(@NonNull Bitmap icon, @NonNull String description) {
        mIcon = icon;
        mDescription = description;
    }

    /**
     * @return Whether this button should be shown on the toolbar.
     */
    public boolean showOnToolbar() {
        return mIsOnToolbar;
    }

    /**
     * @return The id associated with this button. The custom button on the toolbar always uses
     *         {@link CustomTabsIntent#TOOLBAR_ACTION_BUTTON_ID} as id.
     */
    public int getId() {
        return mId;
    }

    /**
     * @return The drawable for the customized button.
     */
    public Drawable getIcon(Context context) {
        if (mShouldTint) {
            return new TintedDrawable(context, mIcon);
        } else {
            return new BitmapDrawable(context.getResources(), mIcon);
        }
    }

    /**
     * @return The content description for the customized button.
     */
    public String getDescription() {
        return mDescription;
    }

    /**
     * @return The {@link PendingIntent} that will be sent when user clicks the customized button.
     */
    public PendingIntent getPendingIntent() {
        return mPendingIntent;
    }

    /**
     * Builds an {@link ImageButton} from the data in this params. Generated buttons should be
     * placed on the bottom bar. The button's tag will be its id.
     * @param parent The parent that the inflated {@link ImageButton}.
     * @param listener {@link OnClickListener} that should be used with the button.
     * @return Parsed list of {@link CustomButtonParams}, which is empty if the input is invalid.
     */
    ImageButton buildBottomBarButton(Context context, ViewGroup parent, OnClickListener listener) {
        assert !mIsOnToolbar;

        ImageButton button = (ImageButton) LayoutInflater.from(context).inflate(
                R.layout.custom_tabs_bottombar_item, parent, false);
        button.setId(mId);
        button.setImageBitmap(mIcon);
        button.setContentDescription(mDescription);
        if (mPendingIntent == null) {
            button.setEnabled(false);
        } else {
            button.setOnClickListener(listener);
        }
        button.setOnLongClickListener(new OnLongClickListener() {
            @Override
            public boolean onLongClick(View view) {
                final int screenWidth = view.getResources().getDisplayMetrics().widthPixels;
                final int screenHeight = view.getResources().getDisplayMetrics().heightPixels;
                final int[] screenPos = new int[2];
                view.getLocationOnScreen(screenPos);
                final int width = view.getWidth();

                Toast toast = Toast.makeText(
                        view.getContext(), view.getContentDescription(), Toast.LENGTH_SHORT);
                toast.setGravity(Gravity.BOTTOM | Gravity.END,
                        screenWidth - screenPos[0] - width / 2, screenHeight - screenPos[1]);
                toast.show();
                return true;
            }
        });
        return button;
    }

    /**
     * Parses a list of {@link CustomButtonParams} from the intent sent by clients.
     * @param intent The intent sent by the client.
     * @return A list of parsed {@link CustomButtonParams}. Return an empty list if input is invalid
     */
    public static List<CustomButtonParams> fromIntent(Context context, Intent intent) {
        List<CustomButtonParams> paramsList = new ArrayList<>(1);
        if (intent == null) return paramsList;

        Bundle singleBundle =
                IntentUtils.safeGetBundleExtra(intent, CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE);
        ArrayList<Bundle> bundleList = IntentUtils.getParcelableArrayListExtra(
                intent, CustomTabsIntent.EXTRA_TOOLBAR_ITEMS);
        boolean tinted = IntentUtils.safeGetBooleanExtra(
                intent, CustomTabsIntent.EXTRA_TINT_ACTION_BUTTON, false);
        if (singleBundle != null) {
            CustomButtonParams singleParams = fromBundle(context, singleBundle, tinted, false);
            if (singleParams != null) paramsList.add(singleParams);
        }
        if (bundleList != null) {
            Set<Integer> ids = new HashSet<>();
            for (Bundle bundle : bundleList) {
                CustomButtonParams params = fromBundle(context, bundle, tinted, true);
                if (params == null) {
                    continue;
                } else if (ids.contains(params.getId())) {
                    Log.e(TAG, "Bottom bar items contain duplicate id: " + params.getId());
                    continue;
                }
                ids.add(params.getId());
                paramsList.add(params);
            }
        }
        return paramsList;
    }

    /**
     * Parses params out of a bundle. Note if a custom button contains a bitmap that does not fit
     * into the toolbar, it will be put to the bottom bar.
     * @param fromList Whether the bundle is contained in a list or it is the single bundle that
     *                 directly comes from the intent.
     */
    private static CustomButtonParams fromBundle(
            Context context, Bundle bundle, boolean tinted, boolean fromList) {
        if (bundle == null) return null;

        if (fromList && !bundle.containsKey(CustomTabsIntent.KEY_ID)) return null;
        int id = IntentUtils.safeGetInt(
                bundle, CustomTabsIntent.KEY_ID, CustomTabsIntent.TOOLBAR_ACTION_BUTTON_ID);

        Bitmap bitmap = parseBitmapFromBundle(bundle);
        if (bitmap == null) {
            Log.e(TAG, "Invalid action button: bitmap not present in bundle!");
            return null;
        }

        String description = parseDescriptionFromBundle(bundle);
        if (TextUtils.isEmpty(description)) {
            Log.e(TAG, "Invalid action button: content description not present in bundle!");
            removeBitmapFromBundle(bundle);
            bitmap.recycle();
            return null;
        }

        boolean onToolbar = id == CustomTabsIntent.TOOLBAR_ACTION_BUTTON_ID
                || IntentUtils.safeGetBoolean(bundle, SHOW_ON_TOOLBAR, false);
        if (onToolbar && !doesIconFitToolbar(context, bitmap)) {
            onToolbar = false;
            Log.w(TAG,
                    "Button's icon not suitable for toolbar, putting it to bottom bar instead."
                            + "See: https://developer.android.com/reference/android/support/customtabs/"
                            + "CustomTabsIntent.html#KEY_ICON");
        }

        PendingIntent pendingIntent =
                IntentUtils.safeGetParcelable(bundle, CustomTabsIntent.KEY_PENDING_INTENT);
        // PendingIntent is a must for buttons on the toolbar, but it's optional for bottom bar.
        if (onToolbar && pendingIntent == null) {
            Log.w(TAG, "Invalid action button on toolbar: pending intent not present in bundle!");
            removeBitmapFromBundle(bundle);
            bitmap.recycle();
            return null;
        }

        return new CustomButtonParams(id, bitmap, description, pendingIntent, tinted, onToolbar);
    }

    /**
     * @return The bitmap contained in the given {@link Bundle}. Will return null if input is
     *         invalid.
     */
    static Bitmap parseBitmapFromBundle(Bundle bundle) {
        if (bundle == null) return null;
        Bitmap bitmap = IntentUtils.safeGetParcelable(bundle, CustomTabsIntent.KEY_ICON);
        if (bitmap == null) return null;
        return bitmap;
    }

    /**
     * Remove the bitmap contained in the given {@link Bundle}. Used when the bitmap is invalid.
     */
    private static void removeBitmapFromBundle(Bundle bundle) {
        if (bundle == null) return;

        try {
            bundle.remove(CustomTabsIntent.KEY_ICON);
        } catch (Throwable t) {
            Log.e(TAG, "Failed to remove icon extra from the intent");
        }
    }

    /**
     * @return The content description contained in the given {@link Bundle}. Will return null if
     *         input is invalid.
     */
    static String parseDescriptionFromBundle(Bundle bundle) {
        if (bundle == null) return null;
        String description = IntentUtils.safeGetString(bundle, CustomTabsIntent.KEY_DESCRIPTION);
        if (TextUtils.isEmpty(description)) return null;
        return description;
    }

    /**
     * @return Whether the given icon's size is suitable to put on toolbar.
     */
    public boolean doesIconFitToolbar(Context context) {
        return doesIconFitToolbar(context, mIcon);
    }

    private static boolean doesIconFitToolbar(Context context, Bitmap bitmap) {
        int height = context.getResources().getDimensionPixelSize(R.dimen.toolbar_icon_height);
        if (bitmap.getHeight() < height) return false;
        int scaledWidth = bitmap.getWidth() / bitmap.getHeight() * height;
        if (scaledWidth > 2 * height) return false;
        return true;
    }
}
