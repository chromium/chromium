// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;

import android.content.Intent;

import androidx.browser.customtabs.CustomTabsSessionToken;

import org.junit.Assert;
import org.mockito.Mockito;

import org.chromium.chrome.browser.IntentHandler;
import org.chromium.components.externalauth.ExternalAuthUtils;

import java.util.concurrent.TimeoutException;

/**
 * Custom ActivityTestRule for all instrumentation tests that require a regular or Incognito {@link
 * CustomTabActivity}.
 */
public class IncognitoCustomTabActivityTestRule extends CustomTabActivityTestRule {
    private boolean mRemoveFirstPartyOverride;
    private boolean mCustomSessionInitiatedForIntent;

    private static void createNewCustomTabSessionForIntent(Intent intent) throws TimeoutException {
        // To emulate first party we create a new session with the session token provided by the
        // |intent|. The session is needed in order to verify if the embedder is 1P or not.
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        connection.newSession(token);
    }

    private static boolean isIntentIncognito(Intent intent) {
        return intent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false);
    }

    @Override
    public void startCustomTabActivityWithIntent(Intent intent) {
        if (isIntentIncognito(intent) && !mCustomSessionInitiatedForIntent) {
            try {
                createNewCustomTabSessionForIntent(intent);
            } catch (TimeoutException e) {
                throw new RuntimeException(e);
            }
        }
        super.startCustomTabActivityWithIntent(intent);
    }

    public void setRemoveFirstPartyOverride() {
        mRemoveFirstPartyOverride = true;
    }

    public void setCustomSessionInitiatedForIntent() {
        mCustomSessionInitiatedForIntent = true;
    }

    public void buildSessionWithHiddenTab(CustomTabsSessionToken token) {
        Assert.assertTrue(CustomTabsConnection.getInstance().newSession(token));
        // Need to set params to reach |CustomTabsConnection#doMayLaunchUrlOnUiThread|.
        CustomTabsConnection.getInstance().mClientManager.setHideDomainForSession(token, true);
        CustomTabsConnection.getInstance().setCanUseHiddenTabForSession(token, true);
    }

    @Override
    protected void before() throws Throwable {
        ExternalAuthUtils spy = Mockito.spy(ExternalAuthUtils.getInstance());
        doAnswer(
                        invocation -> {
                            return !mRemoveFirstPartyOverride;
                        })
                .when(spy)
                .isGoogleSigned(any());
        ExternalAuthUtils.setInstanceForTesting(spy);
        super.before();
    }
}
