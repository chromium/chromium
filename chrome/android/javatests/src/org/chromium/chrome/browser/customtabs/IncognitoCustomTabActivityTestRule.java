// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;

import androidx.browser.customtabs.CustomTabsSessionToken;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.browser.AppHooksModule;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.dependency_injection.ModuleOverridesRule;
import org.chromium.components.externalauth.ExternalAuthUtils;

import java.util.concurrent.TimeoutException;

/**
 * Custom ActivityTestRule for all instrumentation tests that require a regular or Incognito {@link
 * CustomTabActivity}.
 */
public class IncognitoCustomTabActivityTestRule extends CustomTabActivityTestRule {
    private boolean mRemoveFirstPartyOverride;
    private boolean mCustomSessionInitiatedForIntent;

    @Rule
    private final TestRule mModuleOverridesRule =
            new ModuleOverridesRule()
                    .setOverride(AppHooksModule.Factory.class, AppHooksModuleForTest::new);

    /**
     * To load a fake module in tests we need to bypass a check if package name of module is
     * Google-signed. This class overrides this check for testing.
     */
    class AppHooksModuleForTest extends AppHooksModule {
        @Override
        public ExternalAuthUtils provideExternalAuthUtils() {
            return new ExternalAuthUtils() {
                @Override
                public boolean isGoogleSigned(String packageName) {
                    if (mRemoveFirstPartyOverride) return false;
                    return true;
                }
            };
        }
    }

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

    public void buildSessionWithHiddenTab(
            CustomTabsConnection connection, CustomTabsSessionToken token) {
        Assert.assertTrue(connection.newSession(token));
        // Need to set params to reach |CustomTabsConnection#doMayLaunchUrlOnUiThread|.
        connection.mClientManager.setHideDomainForSession(token, true);
        connection.setCanUseHiddenTabForSession(token, true);
    }

    @Override
    public Statement apply(Statement base, Description description) {
        // ModuleOverridesRule must be an outer rule.
        Statement moduleOverridesStatement = mModuleOverridesRule.apply(base, description);
        return super.apply(moduleOverridesStatement, description);
    }
}
