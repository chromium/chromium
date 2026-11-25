// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user.website_approval;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.supervised_user.R;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiContainer;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiController;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiEntry;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiProperties;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiProperties.CloseButtonVisibility;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiProperties.Delegate;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiScope;
import org.chromium.components.browser_ui.adaptive_ui.DismissReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.UserCriticalFeature;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.google_apis.gaia.GaiaId;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.Set;

/** Unit tests for {@link WebsiteApprovalCoordinator} and {@link WebsiteApprovalMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class WebsiteApprovalCoordinatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final UnownedUserDataHost mUnownedUserDataHost = new UnownedUserDataHost();

    @Mock private WindowAndroid mWindow;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private AdaptiveUiController mAdaptiveUiController;
    @Mock private WebsiteApprovalCoordinator.CompletionCallback mCompletionCallback;
    @Mock private Profile mProfile;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;

    private Context mContext;
    private GURL mUrl;
    private Bitmap mFavicon;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.getApplication();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
        mUrl = new GURL("https://www.example.com/blocked");
        mFavicon =
                Bitmap.createBitmap(
                        new int[] {Color.BLUE},
                        /* width= */ 1,
                        /* height= */ 1,
                        Bitmap.Config.ARGB_8888);

        doReturn(new WeakReference<>(mContext)).when(mWindow).getContext();
        doReturn(mUnownedUserDataHost).when(mWindow).getUnownedUserDataHost();

        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        doReturn(mIdentityManager).when(mIdentityServicesProvider).getIdentityManager(mProfile);
        AccountInfo accountInfo =
                new AccountInfo.Builder("alice@example.com", new GaiaId("gaia_id"))
                        .givenName("Alice")
                        .build();
        doReturn(accountInfo).when(mIdentityManager).getPrimaryAccountInfo();
    }

    @Test
    public void testInit_whenBothControllersNull_throwsAssertionError() {
        BottomSheetControllerProvider.setInstanceForTesting(null);

        assertThrows(AssertionError.class, this::createCoordinator);
    }

    @Test
    public void testShow_whenBothControllersPresent_usesAdaptiveUiOnly() {
        WebsiteApprovalSheetContent sheetContent = new WebsiteApprovalSheetContent(mContext);
        WebsiteApprovalMediator mediator = createAdaptiveMediator(sheetContent);

        mediator.show();

        verify(mAdaptiveUiController).show(any(AdaptiveUiEntry.class));
        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    public void testLegacyShow_requestsBottomSheetContent() {
        WebsiteApprovalCoordinator coordinator = createCoordinator();

        coordinator.show();

        verify(mBottomSheetController)
                .requestShowContent(any(WebsiteApprovalSheetContent.class), eq(true));
    }

    @Test
    public void testLegacyShow_whenPrimaryAccountNull_doesNotShow() {
        doReturn(null).when(mIdentityManager).getPrimaryAccountInfo();
        WebsiteApprovalCoordinator coordinator = createCoordinator();

        coordinator.show();

        verify(mBottomSheetController, never())
                .requestShowContent(any(BottomSheetContent.class), eq(true));
        verifyNoInteractions(mCompletionCallback);
    }

    @Test
    public void testLegacyApproveClick_hidesSheetAndCallsOnWebsiteApproved() {
        WebsiteApprovalCoordinator coordinator = createCoordinator();
        coordinator.show();

        WebsiteApprovalSheetContent sheetContent = captureLegacySheetContent();
        sheetContent.getApproveButton().performClick();

        verify(mBottomSheetController)
                .hideContent(
                        eq(sheetContent), eq(true), eq(StateChangeReason.INTERACTION_COMPLETE));
        verify(mCompletionCallback).onWebsiteApproved();
    }

    @Test
    public void testLegacyDenyClick_hidesSheetAndCallsOnWebsiteDenied() {
        WebsiteApprovalCoordinator coordinator = createCoordinator();
        coordinator.show();

        WebsiteApprovalSheetContent sheetContent = captureLegacySheetContent();
        sheetContent.getDenyButton().performClick();

        verify(mBottomSheetController)
                .hideContent(
                        eq(sheetContent), eq(true), eq(StateChangeReason.INTERACTION_COMPLETE));
        verify(mCompletionCallback).onWebsiteDenied();
    }

    @Test
    public void testAdaptiveShow_whenPrimaryAccountNull_doesNotShowEntry() {
        doReturn(null).when(mIdentityManager).getPrimaryAccountInfo();
        WebsiteApprovalSheetContent sheetContent = new WebsiteApprovalSheetContent(mContext);
        WebsiteApprovalMediator mediator = createAdaptiveMediator(sheetContent);

        mediator.show();

        verifyNoInteractions(mAdaptiveUiController);
        verifyNoInteractions(mCompletionCallback);
    }

    @Test
    public void testAdaptiveShow_usesTransientScopeAndAllowedContainers() {
        WebsiteApprovalSheetContent sheetContent = new WebsiteApprovalSheetContent(mContext);
        AdaptiveUiEntry entry = showAdaptiveAndCaptureEntry(sheetContent);

        assertEquals(AdaptiveUiScope.TRANSIENT, entry.getScope());
        assertEquals(
                Set.of(AdaptiveUiContainer.BOTTOM_SHEET, AdaptiveUiContainer.MODAL_DIALOG),
                entry.getAllowedContainers());
    }

    @Test
    public void testAdaptiveShow_hidesInLayoutButtonsAndKeepsGapAboveButtons() {
        WebsiteApprovalSheetContent sheetContent = new WebsiteApprovalSheetContent(mContext);
        View contentView = sheetContent.getContentView();
        contentView.setPaddingRelative(12, 16, 20, 24);
        int approveTopMargin =
                ((ViewGroup.MarginLayoutParams) sheetContent.getApproveButton().getLayoutParams())
                        .topMargin;

        PropertyModel adaptiveModel = showAdaptiveAndCaptureEntry(sheetContent).getPropertyModel();

        assertSame(contentView, adaptiveModel.get(AdaptiveUiProperties.CUSTOM_VIEW));
        assertEquals(View.GONE, sheetContent.getApproveButton().getVisibility());
        assertEquals(View.GONE, sheetContent.getDenyButton().getVisibility());
        assertTrue(approveTopMargin > 0);
        assertEquals(12, contentView.getPaddingStart());
        assertEquals(16, contentView.getPaddingTop());
        assertEquals(20, contentView.getPaddingEnd());
        assertEquals(approveTopMargin, contentView.getPaddingBottom());
    }

    @Test
    public void testAdaptiveShow_disablesGestureDismissalAndOmitsHeaderChrome() {
        WebsiteApprovalSheetContent sheetContent = new WebsiteApprovalSheetContent(mContext);
        PropertyModel adaptiveModel = showAdaptiveAndCaptureEntry(sheetContent).getPropertyModel();

        assertTrue(adaptiveModel.get(AdaptiveUiProperties.GESTURE_DISMISSAL_DISABLED));
        assertNull(adaptiveModel.get(AdaptiveUiProperties.TITLE));
        assertEquals(
                CloseButtonVisibility.HIDDEN,
                adaptiveModel.get(AdaptiveUiProperties.CLOSE_BUTTON_VISIBILITY));
    }

    @Test
    public void testAdaptiveShow_setsAccessibilityPropertiesAndUserCriticalFeature() {
        WebsiteApprovalSheetContent sheetContent = new WebsiteApprovalSheetContent(mContext);
        String contentDescription =
                mContext.getString(R.string.parent_website_approval_content_description);
        assertFalse(contentDescription.isEmpty());

        PropertyModel adaptiveModel = showAdaptiveAndCaptureEntry(sheetContent).getPropertyModel();

        assertEquals(
                contentDescription,
                adaptiveModel.get(AdaptiveUiProperties.DIALOG_CONTENT_DESCRIPTION));
        assertEquals(
                R.string.parent_website_approval_full_height,
                adaptiveModel.get(AdaptiveUiProperties.SHEET_OPENED_ACCESSIBILITY_STRING_ID));
        assertEquals(
                R.string.parent_website_approval_closed,
                adaptiveModel.get(AdaptiveUiProperties.SHEET_CLOSED_ACCESSIBILITY_STRING_ID));
        assertEquals(
                UserCriticalFeature.PARENTAL_APPROVAL,
                adaptiveModel.get(AdaptiveUiProperties.SHEET_USER_CRITICAL));
    }

    @Test
    public void testAdaptiveShow_mapsButtonLabels() {
        WebsiteApprovalSheetContent sheetContent = new WebsiteApprovalSheetContent(mContext);
        String approveText = mContext.getString(R.string.parent_website_approval_approve_button);
        String denyText = mContext.getString(R.string.parent_website_approval_deny_button);
        assertFalse(approveText.isEmpty());
        assertFalse(denyText.isEmpty());

        PropertyModel adaptiveModel = showAdaptiveAndCaptureEntry(sheetContent).getPropertyModel();

        assertEquals(approveText, adaptiveModel.get(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT));
        assertEquals(denyText, adaptiveModel.get(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT));
    }

    @Test
    public void testAdaptivePrimaryClick_callsOnWebsiteApprovedOnce() {
        WebsiteApprovalSheetContent sheetContent = new WebsiteApprovalSheetContent(mContext);
        PropertyModel adaptiveModel = showAdaptiveAndCaptureEntry(sheetContent).getPropertyModel();
        Delegate delegate = assertNonNull(adaptiveModel.get(AdaptiveUiProperties.DELEGATE));

        delegate.onPrimaryButtonClicked();
        delegate.onPrimaryButtonClicked();
        delegate.onSecondaryButtonClicked();

        verify(mCompletionCallback, times(1)).onWebsiteApproved();
        verifyNoMoreInteractions(mCompletionCallback);
    }

    @Test
    public void testAdaptiveSecondaryClick_callsOnWebsiteDeniedOnce() {
        WebsiteApprovalSheetContent sheetContent = new WebsiteApprovalSheetContent(mContext);
        PropertyModel adaptiveModel = showAdaptiveAndCaptureEntry(sheetContent).getPropertyModel();
        Delegate delegate = assertNonNull(adaptiveModel.get(AdaptiveUiProperties.DELEGATE));

        delegate.onSecondaryButtonClicked();
        delegate.onSecondaryButtonClicked();
        delegate.onPrimaryButtonClicked();

        verify(mCompletionCallback, times(1)).onWebsiteDenied();
        verifyNoMoreInteractions(mCompletionCallback);
    }

    @Test
    public void testAdaptiveDismiss_sendsNoResult() {
        WebsiteApprovalSheetContent sheetContent = new WebsiteApprovalSheetContent(mContext);
        PropertyModel adaptiveModel = showAdaptiveAndCaptureEntry(sheetContent).getPropertyModel();
        Delegate delegate = assertNonNull(adaptiveModel.get(AdaptiveUiProperties.DELEGATE));

        delegate.onDismissed(DismissReason.BACK_PRESS);

        verifyNoInteractions(mCompletionCallback);
    }

    private WebsiteApprovalCoordinator createCoordinator() {
        return new WebsiteApprovalCoordinator(
                mWindow, mUrl, mCompletionCallback, mFavicon, mProfile);
    }

    private WebsiteApprovalSheetContent captureLegacySheetContent() {
        ArgumentCaptor<WebsiteApprovalSheetContent> captor =
                ArgumentCaptor.forClass(WebsiteApprovalSheetContent.class);
        verify(mBottomSheetController).requestShowContent(captor.capture(), eq(true));
        return captor.getValue();
    }

    private WebsiteApprovalMediator createAdaptiveMediator(
            WebsiteApprovalSheetContent sheetContent) {
        PropertyModel model =
                new PropertyModel.Builder(WebsiteApprovalProperties.ALL_KEYS)
                        .with(WebsiteApprovalProperties.URL, mUrl)
                        .with(WebsiteApprovalProperties.FAVICON, mFavicon)
                        .build();
        PropertyModelChangeProcessor.create(model, sheetContent, WebsiteApprovalViewBinder::bind);
        return new WebsiteApprovalMediator(
                mCompletionCallback,
                mBottomSheetController,
                mAdaptiveUiController,
                sheetContent,
                model,
                mProfile);
    }

    private AdaptiveUiEntry showAdaptiveAndCaptureEntry(WebsiteApprovalSheetContent sheetContent) {
        WebsiteApprovalMediator mediator = createAdaptiveMediator(sheetContent);
        mediator.show();

        ArgumentCaptor<AdaptiveUiEntry> entryCaptor =
                ArgumentCaptor.forClass(AdaptiveUiEntry.class);
        verify(mAdaptiveUiController).show(entryCaptor.capture());
        return entryCaptor.getValue();
    }
}
