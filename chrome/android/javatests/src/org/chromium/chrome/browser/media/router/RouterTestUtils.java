// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router;

import android.app.Dialog;
import android.view.View;

import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.FragmentManager;

import org.hamcrest.Matchers;

import org.chromium.base.Log;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.CriteriaNotSatisfiedException;
import org.chromium.third_party.android.media.R;

import java.util.ArrayList;
import java.util.concurrent.Callable;

/**
 * Test utils for MediaRouter.
 */
public class RouterTestUtils {
    private static final String TAG = "RouterTestUtils";

    public static View waitForRouteButton(final ChromeActivity activity,
            final String chromecastName, int maxTimeoutMs, int intervalMs) {
        return waitForView(new Callable<View>() {
            @Override
            public View call() {
                Dialog mediaRouteListDialog = getDialog(activity);
                if (mediaRouteListDialog == null) {
                    Log.w(TAG, "Cannot find device selection dialog");
                    return null;
                }
                View mediaRouteList = mediaRouteListDialog.findViewById(R.id.mr_chooser_list);
                if (mediaRouteList == null) {
                    Log.w(TAG, "Cannot find device list");
                    return null;
                }
                ArrayList<View> routesWanted = new ArrayList<View>();
                mediaRouteList.findViewsWithText(
                        routesWanted, chromecastName, View.FIND_VIEWS_WITH_TEXT);
                if (routesWanted.size() == 0) {
                    Log.w(TAG, "Cannot find wanted device");
                    return null;
                }
                Log.i(TAG, "Found wanted device");
                return routesWanted.get(0);
            }
        }, maxTimeoutMs, intervalMs);
    }

    public static Dialog waitForDialog(
            final ChromeActivity activity, int maxTimeoutMs, int intervalMs) {
        try {
            CriteriaHelper.pollUiThread(() -> {
                try {
                    Criteria.checkThat(getDialog(activity), Matchers.notNullValue());
                } catch (Exception e) {
                    throw new CriteriaNotSatisfiedException(e);
                }
            }, maxTimeoutMs, intervalMs);
            return getDialog(activity);
        } catch (Exception e) {
            return null;
        }
    }

    public static Dialog getDialog(ChromeActivity activity) {
        FragmentManager fm = activity.getSupportFragmentManager();
        if (fm == null) return null;
        return ((DialogFragment) fm.findFragmentByTag(
                        "android.support.v7.mediarouter:MediaRouteChooserDialogFragment"))
                .getDialog();
    }

    public static View waitForView(
            final Callable<View> getViewCallable, int maxTimeoutMs, int intervalMs) {
        try {
            CriteriaHelper.pollUiThread(() -> {
                try {
                    Criteria.checkThat(getViewCallable.call(), Matchers.notNullValue());
                } catch (Exception e) {
                    throw new CriteriaNotSatisfiedException(e);
                }
            }, maxTimeoutMs, intervalMs);
            return getViewCallable.call();
        } catch (Exception e) {
            return null;
        }
    }
}
