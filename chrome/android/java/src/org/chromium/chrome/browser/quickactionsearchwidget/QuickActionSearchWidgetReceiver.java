// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quickactionsearchwidget;

import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.firstrun.FirstRunFlowSequencer;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.browser.ui.quickactionsearchwidget.QuickActionSearchWidgetReceiverDelegate;

/**
 * A {@link BroadcastReceiver} that receives and handles the intents that are broadcast when the
 * user interacts with the Quick Action Search Widget.
 */
public class QuickActionSearchWidgetReceiver extends BroadcastReceiver {
    private QuickActionSearchWidgetReceiverDelegate mDelegate;

    @Override
    public void onReceive(final Context context, final Intent intent) {
        if (IntentHandler.wasIntentSenderChrome(intent)) {
            handleIntentFromChrome(context, intent);
        }
    }

    /**
     * If FRE is necessary, this function launches FRE, and the intent is re-broadcast once FRE is
     * complete. If FRE is not necessary, the intent is passed to the delegate where the logic for
     * the widget is contained.
     *
     * @param intent The intent received from the widget.
     */
    private void handleIntentFromChrome(Context context, Intent intent) {
        boolean isFirstRunNecessary = FirstRunFlowSequencer.checkIfFirstRunIsNecessary(
                /*preferLightweightFre=*/false, intent);

        if (isFirstRunNecessary) {
            FirstRunFlowSequencer.launch(context, intent, /*requiresBroadcast=*/
                    true, /*preferLightweightFre=*/false);
        } else {
            getDelegate(context).handleAction(context, intent);
        }
    }

    /**
     * This function lazily initializes and returns the
     * {@link QuickActionSearchWidgetReceiverDelegate}
     * for this instance.
     * <p>
     * We don't initialize the delegate in the constructor because creation of the
     * QuickActionSearchWidgetReceiver is done by the system.
     */
    private QuickActionSearchWidgetReceiverDelegate getDelegate(final Context context) {
        if (mDelegate == null) {
            ComponentName searchComponent = new ComponentName(context, SearchActivity.class);
            ComponentName chromeLauncherComponent =
                    new ComponentName(context, ChromeLauncherActivity.class);

            mDelegate = new QuickActionSearchWidgetReceiverDelegate(
                    searchComponent, chromeLauncherComponent);
        }
        return mDelegate;
    }

    /** Sets a QuickActionSearchWidgetReceiverDelegate to facilitate tests. */
    @VisibleForTesting
    void setDelegateForTesting(QuickActionSearchWidgetReceiverDelegate delegate) {
        mDelegate = delegate;
    }
}
