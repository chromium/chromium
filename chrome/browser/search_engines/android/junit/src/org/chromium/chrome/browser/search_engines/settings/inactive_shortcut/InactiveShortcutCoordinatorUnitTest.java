// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.inactive_shortcut;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.components.search_engines.TemplateUrlTestHelpers.buildMockTemplateUrl;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.AimEligibilityServiceFactory;
import org.chromium.chrome.browser.search_engines.AimEligibilityServiceFactoryJni;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_engines.settings.common.SearchEngineListPreference;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlCategory;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link InactiveShortcutCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config
@EnableFeatures(OmniboxFeatureList.STARTER_PACK_EXPANSION)
public class InactiveShortcutCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;
    @Mock private Profile mProfile;
    @Mock private SearchEngineListPreference mPreference;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private LargeIconBridgeJni mLargeIconBridgeJni;
    @Mock private AimEligibilityServiceFactory.Natives mAimEligibilityNativesMock;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private PrefService mPrefServiceMock;
    @Mock private TemplateUrl mTemplateUrl;

    @Captor private ArgumentCaptor<TemplateUrlServiceObserver> mObserverCaptor;

    private InactiveShortcutCoordinator mCoordinator;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        doReturn(mPrefServiceMock).when(mUserPrefsJniMock).get(any(Profile.class));
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        AimEligibilityServiceFactoryJni.setInstanceForTesting(mAimEligibilityNativesMock);
        LargeIconBridgeJni.setInstanceForTesting(mLargeIconBridgeJni);

        doAnswer(
                        invocation -> {
                            Runnable runnable = invocation.getArgument(0);
                            runnable.run();
                            return null;
                        })
                .when(mTemplateUrlService)
                .runWhenLoaded(any());

        mTemplateUrl = buildMockTemplateUrl("test1", /* prepopulatedId= */ 0);

        List<TemplateUrl> urls = new ArrayList<>();
        urls.add(mTemplateUrl);
        when(mTemplateUrlService.getTemplateUrlsByCategory(
                        TemplateUrlCategory.INACTIVE_SITE_SEARCH))
                .thenReturn(urls);

        mCoordinator =
                new InactiveShortcutCoordinator(
                        mContext, mProfile, mPreference, mModalDialogManager);
    }

    @Test
    public void testInitialization() {
        verify(mPreference).setAdapter(any(InactiveShortcutAdapter.class));
    }

    @Test
    public void testDestroy() {
        mCoordinator.destroy();

        verify(mPreference).setAdapter(eq(null));
    }

    @Test
    public void testListObserverInvalidatesDecorations() {
        verify(mTemplateUrlService).addObserver(mObserverCaptor.capture());
        TemplateUrlServiceObserver observer = mObserverCaptor.getValue();

        TemplateUrl secondTemplateUrl = buildMockTemplateUrl("test2", /* prepopulatedId= */ 0);

        List<TemplateUrl> newUrls = new ArrayList<>();
        newUrls.add(mTemplateUrl);
        newUrls.add(secondTemplateUrl);
        when(mTemplateUrlService.getTemplateUrlsByCategory(
                        TemplateUrlCategory.INACTIVE_SITE_SEARCH))
                .thenReturn(newUrls);

        observer.onTemplateURLServiceChanged();

        // Verify that the preference's invalidateDecorations was called exactly 3 times:
        // 1. clearing the existing list of 1 item.
        // 2. adding the first updated template URL item (test1).
        // 3. adding the second updated template URL item (test2).
        verify(mPreference, times(3)).invalidateDecorations();
    }
}
