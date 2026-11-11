// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.context_sharing.R.id.expanded_content_group;
import static org.chromium.chrome.browser.context_sharing.R.id.web_ui_container;

import android.content.Context;
import android.graphics.Color;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactoryJni;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ui.ActorControlCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicEnablingJni;
import org.chromium.chrome.browser.glic.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_bottom_sheet.CoBrowseComponentProvider.TabSelectionDelegate;
import org.chromium.chrome.browser.tab_bottom_sheet.LegacyResizingPlaceholderCoordinator;
import org.chromium.chrome.browser.tab_bottom_sheet.PeekViewManager;
import org.chromium.chrome.browser.tab_bottom_sheet.ResizingPlaceholderCoordinator;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetContent;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetManager;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetSkeletonCoordinator;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.GlowSpec;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;

import java.util.Arrays;
import java.util.Collections;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link GlicBottomSheetContent}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GlicBottomSheetContentUnitTest {
    private static final long CROSSFADE_ADVANCE_MS =
            GlicBottomSheetFreTransitionController.CROSSFADE_DURATION_MS + 50;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private ActorKeyedService mActorKeyedService;
    @Mock private BottomSheetContent mNextContent;
    @Mock private ActorKeyedServiceFactory.Natives mActorKeyedServiceFactoryJni;
    @Mock private TabBottomSheetManager mTabBottomSheetManager;
    @Mock private TabSelectionDelegate mTabSelectionDelegate;
    @Mock private SettingsCustomTabLauncher mCustomTabLauncher;
    @Mock private GlicEnabling.Natives mGlicEnablingJniMock;

    private Context mContext;
    private View mContentView;
    private GlicBottomSheetContent mContent;
    private GlicBottomSheetComponentProvider mProvider;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
        FrameLayout container = new FrameLayout(mContext);
        View peekContainer = new View(mContext);
        peekContainer.setId(12345);
        container.addView(peekContainer);

        mContentView = container;

        ActorKeyedServiceFactoryJni.setInstanceForTesting(mActorKeyedServiceFactoryJni);
        ActorKeyedServiceFactory.setForTesting(mActorKeyedService);
        GlicEnablingJni.setInstanceForTesting(mGlicEnablingJniMock);

        mContent =
                new GlicBottomSheetContent(
                        mContentView,
                        0.7f,
                        1.0f,
                        0xFFFFFFFF,
                        /* peekViewHeight= */ 100,
                        /* peekViewContainerId= */ 12345,
                        /* onBackPressed= */ () -> {},
                        mProfile);
        mProvider = new GlicBottomSheetComponentProvider(mProfile, null);
    }

    @Test
    public void testConstructor_InitializesCorrectly() {
        assertEquals(mContentView, mContent.getContentView());
        assertEquals(
                BottomSheetContent.HeightMode.WRAP_CONTENT, mContent.getFullHeightRatio(), 0.001f);
    }

    @Test
    public void testSheetBackgroundGlowSpecOverride() {
        GlowSpec glowSpec = mContent.getSheetBackgroundGlowSpecOverride();
        assertNotNull(glowSpec);
        assertEquals(mContext.getColor(R.color.default_bg_color_blue), glowSpec.color);
        assertEquals(GlowSpec.ShadowSize.LONG, glowSpec.size);
    }

    @Test
    public void testCanBeSuppressed_NoService() {
        ActorKeyedServiceFactory.setForTesting(null);
        when(mActorKeyedServiceFactoryJni.getForProfile(mProfile)).thenReturn(null);
        assertTrue(mContent.canBeSuppressed(mNextContent));
    }

    @Test
    public void testCanBeSuppressed_NoActiveTasks() {
        when(mActorKeyedService.getActiveTasks()).thenReturn(null);
        assertTrue(mContent.canBeSuppressed(mNextContent));

        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.emptyList());
        assertTrue(mContent.canBeSuppressed(mNextContent));
    }

    @Test
    public void testCanBeSuppressed_ActiveTaskUnderActorControl() {
        ActorTask mockTask = mock(ActorTask.class);
        when(mockTask.isUnderActorControl()).thenReturn(true);
        when(mockTask.isCompleted()).thenReturn(false);

        when(mActorKeyedService.getActiveTasks()).thenReturn(Arrays.asList(mockTask));
        assertFalse(mContent.canBeSuppressed(mNextContent));
    }

    @Test
    public void testCanBeSuppressed_ActiveTaskCompleted() {
        ActorTask mockTask = mock(ActorTask.class);
        when(mockTask.isUnderActorControl()).thenReturn(true);
        when(mockTask.isCompleted()).thenReturn(true);

        when(mActorKeyedService.getActiveTasks()).thenReturn(Arrays.asList(mockTask));
        assertTrue(mContent.canBeSuppressed(mNextContent));
    }

    @Test
    public void testCanBeSuppressed_ActiveTaskNotUnderActorControl() {
        ActorTask mockTask = mock(ActorTask.class);
        when(mockTask.isUnderActorControl()).thenReturn(false);
        when(mockTask.isCompleted()).thenReturn(false);

        when(mActorKeyedService.getActiveTasks()).thenReturn(Arrays.asList(mockTask));
        assertTrue(mContent.canBeSuppressed(mNextContent));
    }

    @Test
    public void testCanBeSuppressed_MixedTasks_SuppressedIfAnyUnderActorControl() {
        ActorTask finishedTask = mock(ActorTask.class);
        when(finishedTask.isUnderActorControl()).thenReturn(true);
        when(finishedTask.isCompleted()).thenReturn(true);

        ActorTask passiveTask = mock(ActorTask.class);
        when(passiveTask.isUnderActorControl()).thenReturn(false);
        when(passiveTask.isCompleted()).thenReturn(false);

        ActorTask activeTask = mock(ActorTask.class);
        when(activeTask.isUnderActorControl()).thenReturn(true);
        when(activeTask.isCompleted()).thenReturn(false);

        when(mActorKeyedService.getActiveTasks())
                .thenReturn(Arrays.asList(finishedTask, passiveTask, activeTask));
        assertFalse(mContent.canBeSuppressed(mNextContent));
    }

    @Test
    public void testProvider_ConfiguresPlaceholderCorrectly() {
        TextViewWithCompoundDrawables placeholder = new TextViewWithCompoundDrawables(mContext);
        assertTrue(mProvider.setupPlaceholderView(placeholder));
        assertEquals(
                mContext.getString(R.string.glic_inactive_view_card_text),
                placeholder.getText().toString());
        // Top compound drawable (index 1) must be set.
        assertNotNull(placeholder.getCompoundDrawablesRelative()[1]);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW)
    public void testProvider_createResizingPlaceholderCoordinator_defaultLegacy() {
        ResizingPlaceholderCoordinator coordinator =
                mProvider.createResizingPlaceholderCoordinator(mContext, Color.WHITE, Color.LTGRAY);
        assertTrue(coordinator instanceof LegacyResizingPlaceholderCoordinator);
        coordinator.destroy();
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_BOTTOM_SHEET,
        ChromeFeatureList.TAB_BOTTOM_SHEET_RESIZE_WEBVIEW
                + ":"
                + TabBottomSheetUtils.RESIZING_PLACEHOLDER_PARAM
                + "/"
                + TabBottomSheetUtils.PLACEHOLDER_SKELETON
    })
    public void testProvider_createResizingPlaceholderCoordinator_skeletonEnabled() {
        ResizingPlaceholderCoordinator coordinator =
                mProvider.createResizingPlaceholderCoordinator(mContext, Color.WHITE, Color.LTGRAY);
        assertTrue(coordinator instanceof TabBottomSheetSkeletonCoordinator);
        coordinator.destroy();
    }

    @Test
    public void testProvider_createContent() {
        TabBottomSheetContent createdContent =
                mProvider.createContent(
                        mContentView,
                        /* defaultHeightRatio= */ 0.7f,
                        /* fullHeightRatio= */ 1.0f,
                        Color.WHITE,
                        /* peekViewHeight= */ 100,
                        /* peekViewContainerId= */ 12345,
                        () -> {});
        assertTrue(createdContent instanceof GlicBottomSheetContent);
    }

    @Test
    public void testProvider_createPeekViewManager() {
        SettableMonotonicObservableSupplier<Profile> profileSupplier =
                ObservableSuppliers.createMonotonic();
        SettableNullableObservableSupplier<Tab> tabSupplier = ObservableSuppliers.createNullable();
        PeekViewManager peekViewManager =
                mProvider.createPeekViewManager(
                        mTabBottomSheetManager,
                        profileSupplier,
                        tabSupplier,
                        mTabSelectionDelegate);
        assertTrue(peekViewManager instanceof ActorControlCoordinator);

        peekViewManager.destroy();
        mProvider.destroy();
    }

    private GlicBottomSheetContent createContentWithNativeFre(
            ViewGroup expandedGroup, Runnable onAccepted, Runnable onBackPressed) {
        FrameLayout container = new FrameLayout(mContext);
        View peekContainer = new View(mContext);
        peekContainer.setId(12345);
        container.addView(peekContainer);
        expandedGroup.setId(expanded_content_group);
        container.addView(expandedGroup);
        FrameLayout webUiContainer = new FrameLayout(mContext);
        webUiContainer.setId(web_ui_container);
        expandedGroup.addView(webUiContainer);

        GlicBottomSheetContent content =
                new GlicBottomSheetContent(
                        container,
                        0.7f,
                        1.0f,
                        0xFFFFFFFF,
                        /* peekViewHeight= */ 100,
                        /* peekViewContainerId= */ 12345,
                        onBackPressed,
                        mProfile);

        content.mountNativeFre(mCustomTabLauncher, onAccepted);
        return content;
    }

    @Test
    public void testNativeFre_AcceptCrossfadesOutBeforeTeardown() {
        FrameLayout expandedGroup = new FrameLayout(mContext);
        Runnable onAccepted = mock(Runnable.class);
        Runnable onBackPressed = mock(Runnable.class);
        GlicBottomSheetContent content =
                createContentWithNativeFre(expandedGroup, onAccepted, onBackPressed);
        View freRoot = expandedGroup.findViewById(R.id.glic_fre_root);
        assertNotNull(freRoot);
        doAnswer(
                        invocation -> {
                            assertTrue(content.isNativeFreShowingForTesting());
                            assertFalse(freRoot.isClickable());
                            return null;
                        })
                .when(onAccepted)
                .run();

        expandedGroup.findViewById(R.id.glic_fre_accept_button).performClick();
        verify(onAccepted).run();
        assertTrue(content.isNativeFreShowingForTesting());
        verifyNoInteractions(onBackPressed);

        ShadowLooper.idleMainLooper(CROSSFADE_ADVANCE_MS, TimeUnit.MILLISECONDS);
        assertFalse(content.isNativeFreShowingForTesting());
        assertNull(expandedGroup.findViewById(R.id.glic_fre_root));

        content.destroy();
    }

    @Test
    public void testNativeFre_DestroyDuringCrossfadeRemovesImmediately() {
        FrameLayout expandedGroup = new FrameLayout(mContext);
        Runnable onAccepted = mock(Runnable.class);
        Runnable onBackPressed = mock(Runnable.class);

        GlicBottomSheetContent content =
                createContentWithNativeFre(expandedGroup, onAccepted, onBackPressed);

        View acceptButton = expandedGroup.findViewById(R.id.glic_fre_accept_button);
        assertNotNull(acceptButton);
        acceptButton.performClick();
        verify(onAccepted).run();
        assertTrue(content.isNativeFreShowingForTesting());

        content.destroy();
        assertFalse(content.isNativeFreShowingForTesting());
        assertNull(expandedGroup.findViewById(R.id.glic_fre_accept_button));
        verifyNoInteractions(onBackPressed);
    }

    @Test
    public void testNativeFre_TeardownDuringCrossfadeDoesNotTearDownRemountedFre() {
        FrameLayout expandedGroup = new FrameLayout(mContext);
        GlicBottomSheetContent content =
                createContentWithNativeFre(expandedGroup, () -> {}, () -> {});
        expandedGroup.findViewById(R.id.glic_fre_accept_button).performClick();

        content.teardownNativeFre();
        content.mountNativeFre(mCustomTabLauncher, () -> {});
        ShadowLooper.idleMainLooper(CROSSFADE_ADVANCE_MS, TimeUnit.MILLISECONDS);

        assertTrue(content.isNativeFreShowingForTesting());
        assertNotNull(expandedGroup.findViewById(R.id.glic_fre_accept_button));
        content.destroy();
    }

    @Test
    public void testNativeFre_HidesWebUiUntilFadeOutEnds() {
        FrameLayout expandedGroup = new FrameLayout(mContext);
        GlicBottomSheetContent content =
                createContentWithNativeFre(expandedGroup, () -> {}, () -> {});
        ViewGroup webUi = expandedGroup.findViewById(web_ui_container);

        expandedGroup.findViewById(R.id.glic_fre_accept_button).performClick();
        assertEquals(
                View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS,
                webUi.getImportantForAccessibility());
        assertEquals(ViewGroup.FOCUS_BLOCK_DESCENDANTS, webUi.getDescendantFocusability());

        ShadowLooper.idleMainLooper(CROSSFADE_ADVANCE_MS, TimeUnit.MILLISECONDS);
        assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_AUTO, webUi.getImportantForAccessibility());
        assertEquals(ViewGroup.FOCUS_BEFORE_DESCENDANTS, webUi.getDescendantFocusability());

        content.destroy();
    }

    @Test
    public void testNativeFre_TeardownShowsWebUi() {
        FrameLayout expandedGroup = new FrameLayout(mContext);
        GlicBottomSheetContent content =
                createContentWithNativeFre(expandedGroup, () -> {}, () -> {});
        ViewGroup webUi = expandedGroup.findViewById(web_ui_container);
        assertEquals(ViewGroup.FOCUS_BLOCK_DESCENDANTS, webUi.getDescendantFocusability());

        content.teardownNativeFre();
        assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_AUTO, webUi.getImportantForAccessibility());
        assertEquals(ViewGroup.FOCUS_BEFORE_DESCENDANTS, webUi.getDescendantFocusability());

        content.destroy();
    }

    @Test
    public void testNativeFre_CloseButtonTriggersBackPress() {
        FrameLayout expandedGroup = new FrameLayout(mContext);
        Runnable onAccepted = mock(Runnable.class);
        Runnable onBackPressed = mock(Runnable.class);

        GlicBottomSheetContent content =
                createContentWithNativeFre(expandedGroup, onAccepted, onBackPressed);

        View closeButton = expandedGroup.findViewById(R.id.glic_fre_close_button);
        assertNotNull(closeButton);
        closeButton.performClick();
        verify(onBackPressed).run();
        verifyNoInteractions(onAccepted);

        content.destroy();
    }

    @Test
    public void testNativeFre_DestroyCleansUpWithoutCallbacks() {
        FrameLayout expandedGroup = new FrameLayout(mContext);
        Runnable onAccepted = mock(Runnable.class);
        Runnable onBackPressed = mock(Runnable.class);

        GlicBottomSheetContent content =
                createContentWithNativeFre(expandedGroup, onAccepted, onBackPressed);

        assertTrue(content.isNativeFreShowingForTesting());
        content.destroy();
        verifyNoInteractions(onBackPressed);
        verifyNoInteractions(onAccepted);
        assertFalse(content.isNativeFreShowingForTesting());
        assertNull(expandedGroup.findViewById(R.id.glic_fre_accept_button));
    }

    @Test
    public void testNativeFre_MountTwiceIsIdempotent() {
        FrameLayout expandedGroup = new FrameLayout(mContext);
        GlicBottomSheetContent content =
                createContentWithNativeFre(
                        expandedGroup, /* onAccepted= */ () -> {}, /* onBackPressed= */ () -> {});

        assertEquals(2, expandedGroup.getChildCount());

        content.mountNativeFre(mCustomTabLauncher, /* onNativeFreAccepted= */ () -> {});

        assertEquals(2, expandedGroup.getChildCount());
        verify(mGlicEnablingJniMock).isEnterpriseAccount(mProfile);

        content.destroy();
    }

    @Test
    public void testNativeFre_GetVerticalScrollOffsetTracksFreScrollView() {
        FrameLayout expandedGroup = new FrameLayout(mContext);
        GlicBottomSheetContent content =
                createContentWithNativeFre(expandedGroup, () -> {}, () -> {});

        View scrollView = expandedGroup.findViewById(R.id.glic_fre_scroll_view);
        scrollView.layout(0, 0, 400, 300);
        ((ViewGroup) scrollView).getChildAt(0).layout(0, 0, 400, 600);
        scrollView.setScrollY(75);
        assertEquals(75, content.getVerticalScrollOffset());

        content.teardownNativeFre();
        assertEquals(0, content.getVerticalScrollOffset());

        content.destroy();
    }
}
