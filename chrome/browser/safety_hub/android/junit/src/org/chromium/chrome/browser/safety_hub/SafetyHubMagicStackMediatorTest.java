// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.view.View;
import android.view.View.OnClickListener;

import androidx.annotation.Nullable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for the Safety Hub Magic Stack mediator. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class SafetyHubMagicStackMediatorTest {
    private static final String DESCRIPTION = "description";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MagicStackBridge mMagicStackBridge;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private ModuleDelegate mModuleDelegate;
    @Mock private SettingsLauncher mSettingsLauncher;
    @Mock private View mView;

    private Context mContext;
    private PropertyModel mModel;
    private SafetyHubMagicStackMediator mMediator;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        mModel = new PropertyModel(SafetyHubMagicStackViewProperties.ALL_KEYS);
        mMediator =
                new SafetyHubMagicStackMediator(
                        mContext,
                        mModel,
                        mMagicStackBridge,
                        mTabModelSelector,
                        mModuleDelegate,
                        mSettingsLauncher);

        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
    }

    @Test
    public void testNothingToDisplay() {
        doReturn(null).when(mMagicStackBridge).getModuleToShow();
        mMediator.showModule();
        verify(mModuleDelegate).onDataFetchFailed(ModuleType.SAFETY_HUB);
    }

    @Test
    public void testSafeBrowsingDisplayed() {
        MagicStackEntry entry =
                MagicStackEntry.create(DESCRIPTION, MagicStackEntry.ModuleType.SAFE_BROWSING);
        doReturn(entry).when(mMagicStackBridge).getModuleToShow();

        mMediator.showModule();

        verify(mModuleDelegate).onDataReady(eq(ModuleType.SAFETY_HUB), eq(mModel));
        assertEquals(
                mModel.get(SafetyHubMagicStackViewProperties.HEADER),
                mContext.getResources().getString(R.string.safety_hub_magic_stack_module_name));
        assertEquals(
                mModel.get(SafetyHubMagicStackViewProperties.TITLE),
                mContext.getResources()
                        .getString(R.string.safety_hub_magic_stack_safe_browsing_title));
        assertEquals(mModel.get(SafetyHubMagicStackViewProperties.SUMMARY), DESCRIPTION);
        assertEquals(
                mModel.get(SafetyHubMagicStackViewProperties.BUTTON_TEXT),
                mContext.getResources()
                        .getString(R.string.safety_hub_magic_stack_safe_browsing_button_text));
        assertEquals(
                shadowOf(mModel.get(SafetyHubMagicStackViewProperties.ICON_DRAWABLE))
                        .getCreatedFromResId(),
                R.drawable.ic_gshield_24);

        OnClickListener onClickListener =
                mModel.get(SafetyHubMagicStackViewProperties.BUTTON_ON_CLICK_LISTENER);
        onClickListener.onClick(mView);
        verify(mSettingsLauncher)
                .launchSettingsActivity(eq(mContext), eq(SafeBrowsingSettingsFragment.class));
    }

    @Test
    public void testRevokedPermisisonsDisplayed() {
        MagicStackEntry entry =
                MagicStackEntry.create(DESCRIPTION, MagicStackEntry.ModuleType.REVOKED_PERMISSIONS);
        doReturn(entry).when(mMagicStackBridge).getModuleToShow();
        mMediator.showModule();

        testSafeStateDisplayed(DESCRIPTION, null);
    }

    @Test
    public void testNotificationPermissionsDisplayed() {
        MagicStackEntry entry =
                MagicStackEntry.create(
                        DESCRIPTION, MagicStackEntry.ModuleType.NOTIFICATION_PERMISSIONS);
        doReturn(entry).when(mMagicStackBridge).getModuleToShow();
        mMediator.showModule();

        testSafeStateDisplayed(
                mContext.getResources()
                        .getString(R.string.safety_hub_magic_stack_notifications_title),
                DESCRIPTION);
    }

    private void testSafeStateDisplayed(String title, @Nullable String summary) {
        verify(mModuleDelegate).onDataReady(eq(ModuleType.SAFETY_HUB), eq(mModel));
        assertEquals(
                mModel.get(SafetyHubMagicStackViewProperties.HEADER),
                mContext.getResources().getString(R.string.safety_hub_magic_stack_module_name));
        assertEquals(mModel.get(SafetyHubMagicStackViewProperties.TITLE), title);
        assertEquals(mModel.get(SafetyHubMagicStackViewProperties.SUMMARY), summary);
        assertEquals(
                mModel.get(SafetyHubMagicStackViewProperties.BUTTON_TEXT),
                mContext.getResources()
                        .getString(R.string.safety_hub_magic_stack_safe_state_button_text));
        assertEquals(
                shadowOf(mModel.get(SafetyHubMagicStackViewProperties.ICON_DRAWABLE))
                        .getCreatedFromResId(),
                R.drawable.ic_check_circle_filled_green_24dp);

        OnClickListener onClickListener =
                mModel.get(SafetyHubMagicStackViewProperties.BUTTON_ON_CLICK_LISTENER);
        onClickListener.onClick(mView);
        verify(mSettingsLauncher).launchSettingsActivity(eq(mContext), eq(SafetyHubFragment.class));
    }
}
