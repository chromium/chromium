// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetUtils.isActivityFinishingOrDestroyed;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.view.View;
import android.view.ViewTreeObserver;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.intents.BrowserIntentUtils;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewAttachParams;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.components.thinwebview.internal.ThinWebViewContextMenuItemDelegate;
import org.chromium.content_public.browser.ActionModeCallbackHelper;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.selection.SelectionDropdownMenuDelegate;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.url.GURL;

import java.util.function.BiConsumer;

/** Abstract class for Tab Bottom Sheet toolbars. */
@NullMarked
public class TabBottomSheetWebUi {
    private static boolean sInTestMode;

    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final ContextMenuPopulatorFactory mContextMenuPopulatorFactory;
    private final SelectionDropdownMenuDelegate mSelectionDropdownMenuDelegate;
    private final WebViewResizingHelper mWebViewResizingHelper;
    private final @ColorInt int mBackgroundColor;
    private final @TabBottomSheetClientType int mClientType;
    private final @CoBrowseContainerType int mContainerType;
    private final @Nullable BiConsumer<GURL, String> mEphemeralTabOpener;
    private final @Nullable BiConsumer<GURL, String> mReadLaterOpener;

    private @Nullable ThinWebView mThinWebView;
    private @Nullable WebContents mWebContents;
    private @Nullable ContentView mContentView;

    // When the user highlights text and shows the dropdown menu, the dropdown window captures
    // focus, causing the activity window to temporarily lose focus. We ignore clearing focus
    // in this state to preserve the highlighted selection, but ensure focus is cleared when the
    // dropdown is dismissed if the window has not regained focus.
    private boolean mIgnoreClearFocusForDropdown;

    TabBottomSheetWebUi(
            Context context,
            View containerView,
            WindowAndroid windowAndroid,
            ContextMenuPopulatorFactory contextMenuPopulatorFactory,
            SelectionDropdownMenuDelegate selectionDropdownMenuDelegate,
            @ColorInt int backgroundColor,
            @TabBottomSheetClientType int clientType,
            @CoBrowseContainerType int containerType,
            @Nullable BiConsumer<GURL, String> ephemeralTabOpener,
            @Nullable BiConsumer<GURL, String> readLaterOpener) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mContextMenuPopulatorFactory = contextMenuPopulatorFactory;
        mSelectionDropdownMenuDelegate =
                new SelectionDropdownMenuDelegateWrapper(selectionDropdownMenuDelegate);
        mBackgroundColor = backgroundColor;
        mClientType = clientType;
        mContainerType = containerType;
        mEphemeralTabOpener = ephemeralTabOpener;
        mReadLaterOpener = readLaterOpener;
        mWebViewResizingHelper =
                new WebViewResizingHelper(
                        containerView,
                        windowAndroid,
                        backgroundColor,
                        containerType == CoBrowseContainerType.SIDE_PANEL);
    }

    @SuppressLint("ClickableViewAccessibility")
    void setWebContents(@Nullable WebContents webContents, boolean requestFocus) {
        if (mWebContents == webContents) {
            return;
        }

        // Reset references and top-level window for the old WebContents and ContentView.
        mContentView = null;

        if (mWebContents != null && !mWebContents.isDestroyed()) {
            mWebContents.setTopLevelNativeWindow(null);
        }

        mWebContents = webContents;
        if (mWebContents == null) {
            destroyThinWebView();
            return;
        }

        mWebContents.getEventForwarder().setCurrentTouchOffsetX(0.0f);
        mWebContents.getEventForwarder().setCurrentTouchOffsetY(0.0f);
        // Use a local variable to ensure we are using the correct ContentView instance.
        ContentView contentView = createContentView(mContext, mWebContents);
        mContentView = contentView;

        contentView.addOnAttachStateChangeListener(
                new View.OnAttachStateChangeListener() {
                    private final ViewTreeObserver.OnWindowFocusChangeListener mListener =
                            new ViewTreeObserver.OnWindowFocusChangeListener() {
                                @Override
                                public void onWindowFocusChanged(boolean hasFocus) {
                                    if (!hasFocus && !mIgnoreClearFocusForDropdown) {
                                        contentView.clearFocus();
                                    }
                                }
                            };

                    @Override
                    public void onViewAttachedToWindow(View v) {
                        contentView.getViewTreeObserver().addOnWindowFocusChangeListener(mListener);
                    }

                    @Override
                    public void onViewDetachedFromWindow(View v) {
                        contentView
                                .getViewTreeObserver()
                                .removeOnWindowFocusChangeListener(mListener);
                    }
                });
        // Most systems assume ViewAndroidDelegate is created alongside WebContents and never
        // changes. SelectionPopupControllerImpl is an example of a system that does this so if
        // we don't reuse the existing delegate, popups will break.
        ViewAndroidDelegate viewDelegate = mWebContents.getViewAndroidDelegate();
        if (viewDelegate == null) {
            mWebContents.setDelegates(
                    VersionInfo.getProductVersion(),
                    ViewAndroidDelegate.createBasicDelegate(contentView),
                    contentView,
                    mWindowAndroid,
                    WebContents.createDefaultInternalsHolder());
        } else {
            // This mirrors the internal updates that happen in setDelegates for the things
            // that may have changed (contentView and WindowAndroid).
            mWebContents.setTopLevelNativeWindow(mWindowAndroid);
            viewDelegate.setContainerView(contentView);

            // Working with this in a test is impossible as ViewEventSinkImpl is final and
            // WebContentsImpl is not reachable to mock. As such, we need to skip this step in
            // test mode.
            if (!sInTestMode) {
                ViewEventSink.from(mWebContents).setAccessDelegate(contentView);
            }
        }
        ThinWebViewContextMenuItemDelegate.LinkOpener linkOpener =
                new ThinWebViewContextMenuItemDelegate.LinkOpener() {
                    private void safeStartActivity(Intent intent) {
                        Activity activity = mWindowAndroid.getActivity().get();
                        if (activity != null) {
                            intent.setPackage(
                                    ContextUtils.getApplicationContext().getPackageName());
                            IntentUtils.addTrustedIntentExtras(intent);
                            activity.startActivity(intent);
                        }
                    }

                    @Override
                    public void openInNewTab(GURL url) {
                        Intent intent = new Intent(Intent.ACTION_VIEW);
                        intent.setData(Uri.parse(url.getSpec()));
                        safeStartActivity(intent);
                    }

                    @Override
                    public void openInNewTabInGroup(GURL url) {
                        TabModelSelector selector =
                                TabModelSelectorSupplier.getValueOrNullFrom(mWindowAndroid);
                        Tab currentTab = TabModelSelectorSupplier.getCurrentTabFrom(mWindowAndroid);
                        if (selector != null && currentTab != null) {
                            LoadUrlParams loadUrlParams = new LoadUrlParams(url.getSpec());
                            selector.openNewTab(
                                    loadUrlParams,
                                    TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                                    currentTab,
                                    currentTab.isIncognito());
                        } else {
                            Intent intent = new Intent(Intent.ACTION_VIEW);
                            intent.setData(Uri.parse(url.getSpec()));
                            intent.putExtra(
                                    BrowserIntentUtils.EXTRA_TAB_LAUNCH_TYPE,
                                    TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP);
                            safeStartActivity(intent);
                        }
                    }

                    @Override
                    public void openInNewIncognitoTab(GURL url) {
                        Intent intent = new Intent(Intent.ACTION_VIEW);
                        intent.setData(Uri.parse(url.getSpec()));
                        intent.putExtra(BrowserIntentUtils.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
                        safeStartActivity(intent);
                    }

                    @Override
                    public void openInNewWindow(GURL url) {
                        Activity activity = mWindowAndroid.getActivity().get();
                        if (activity != null) {
                            LoadUrlParams loadUrlParams = new LoadUrlParams(url.getSpec());
                            MultiInstanceOrchestratorFactory.getInstance()
                                    .openUrlInOtherWindow(
                                            activity,
                                            loadUrlParams,
                                            Tab.INVALID_TAB_ID,
                                            /* preferNew= */ true,
                                            /* isIncognito= */ false);
                        }
                    }

                    @Override
                    public void openInIncognitoWindow(GURL url) {
                        Activity activity = mWindowAndroid.getActivity().get();
                        if (activity != null) {
                            LoadUrlParams loadUrlParams = new LoadUrlParams(url.getSpec());
                            MultiInstanceOrchestratorFactory.getInstance()
                                    .openUrlInOtherWindow(
                                            activity,
                                            loadUrlParams,
                                            Tab.INVALID_TAB_ID,
                                            /* preferNew= */ false,
                                            /* isIncognito= */ true);
                        }
                    }

                    @Override
                    public boolean isIncognitoSupported() {
                        if (mWebContents == null) return false;
                        Profile profile = Profile.fromWebContents(mWebContents);
                        return profile != null && IncognitoUtils.isIncognitoModeEnabled(profile);
                    }
                };

        ThinWebViewContextMenuItemDelegate itemDelegate =
                new ThinWebViewContextMenuItemDelegate(
                        mWebContents,
                        mContainerType == CoBrowseContainerType.SIDE_PANEL
                                ? BrowserIntentUtils.CHROME_LAUNCHER_ACTIVITY_CLASS_NAME
                                : null,
                        mEphemeralTabOpener,
                        mContainerType == CoBrowseContainerType.SIDE_PANEL ? linkOpener : null,
                        mReadLaterOpener);
        mContextMenuPopulatorFactory.setItemDelegate(itemDelegate);
        ensureThinWebViewCreated();
        if (mThinWebView != null) {
            mThinWebView.attachWebContents(
                    mWebContents,
                    contentView,
                    new ThinWebViewAttachParams.Builder()
                            .setContextMenuPopulatorFactory(mContextMenuPopulatorFactory)
                            .setSelectionDropdownMenuDelegate(mSelectionDropdownMenuDelegate)
                            .setSupportTheming(true)
                            .build());
            if (mClientType == TabBottomSheetClientType.CONTEXTUAL_TASKS) {
                // This disables ActionModeSelectionMenu from ever being shown on AIM.
                // TODO (crbug.com/534284847): How to handle ActionModeSelectionMenu on non-AL
                // devices.
                SelectionPopupController.fromWebContents(mWebContents)
                        .setActionModeCallback(ActionModeCallbackHelper.EMPTY_CALLBACK);
            }
            mWebViewResizingHelper.setThinWebView(mThinWebView, mWebContents);
            setAllowFullscreenIme(
                    mContext.getResources().getConfiguration().orientation
                            == Configuration.ORIENTATION_LANDSCAPE);
        }

        if (requestFocus) {
            // Only request focus once the web contents have been attached to the activity's
            // layout tree.
            View currentlyFocusedView =
                    assertNonNull(mWindowAndroid.getActivity().get()).getCurrentFocus();
            if (currentlyFocusedView != null) {
                currentlyFocusedView.clearFocus();
            }
            contentView.requestFocus();
        }
    }

    void setAllowFullscreenIme(boolean allow) {
        if (mWebContents == null) return;
        ImeAdapter adapter = ImeAdapter.fromWebContents(mWebContents);
        if (adapter != null) {
            adapter.setAllowFullscreenIme(allow);
        }
    }

    @Nullable WebContents getWebContents() {
        return mWebContents;
    }

    void setIgnoreClearFocus(boolean ignoreClearFocus) {
        if (mContentView != null) {
            mContentView.setIgnoreClearFocus(ignoreClearFocus);
        }
    }

    WebViewResizingHelper getWebViewResizingHelper() {
        return mWebViewResizingHelper;
    }

    void destroy() {
        setWebContents(null, false);
    }

    View getWebUiView() {
        return mWebViewResizingHelper.getResizingContainer();
    }

    @VisibleForTesting
    ContentView createContentView(Context context, WebContents webContents) {
        return ContentView.createContentView(context, webContents);
    }

    private void ensureThinWebViewCreated() {
        if (mThinWebView != null) {
            return;
        }

        if (isActivityFinishingOrDestroyed(mWindowAndroid)) {
            return;
        }

        ThinWebViewConstraints constraints = new ThinWebViewConstraints();
        constraints.supportsOpacity = true;
        constraints.backgroundColor = mBackgroundColor;
        constraints.ignoreSizeChanges = true;
        mThinWebView =
                ThinWebViewFactory.create(
                        mContext,
                        constraints,
                        assumeNonNull(mWindowAndroid.getIntentRequestTracker()),
                        /* enablePermissionRequests= */ true);
        mWebViewResizingHelper.setThinWebView(mThinWebView, mWebContents);
    }

    private void destroyThinWebView() {
        if (mThinWebView != null) {
            mThinWebView.destroy();
            mThinWebView = null;
        }
        mWebViewResizingHelper.reset();
        mContentView = null;
    }

    @Nullable ThinWebView getThinWebViewForTesting() {
        return mThinWebView;
    }

    static void setInTestModeForTesting() {
        sInTestMode = true;
        ResettersForTesting.register(() -> sInTestMode = false);
    }

    private class SelectionDropdownMenuDelegateWrapper implements SelectionDropdownMenuDelegate {
        private final SelectionDropdownMenuDelegate mDelegate;

        public SelectionDropdownMenuDelegateWrapper(SelectionDropdownMenuDelegate delegate) {
            mDelegate = delegate;
        }

        @Override
        public void show(
                Context context,
                View rootView,
                MVCListAdapter.ModelList items,
                ItemClickListener clickListener,
                Runnable dismissMenuCallback,
                int x,
                int y) {
            mIgnoreClearFocusForDropdown = true;
            mDelegate.show(
                    context,
                    rootView,
                    items,
                    clickListener,
                    () -> {
                        mIgnoreClearFocusForDropdown = false;
                        dismissMenuCallback.run();
                    },
                    x,
                    y);
        }

        @Override
        public void dismiss() {
            mIgnoreClearFocusForDropdown = false;
            mDelegate.dismiss();
            if (mContentView != null && !mContentView.hasWindowFocus()) {
                mContentView.clearFocus();
            }
        }

        @Override
        public ListItem getDivider() {
            return mDelegate.getDivider();
        }

        @Override
        public ListItem getMenuItem(
                @Nullable String title,
                @Nullable String contentDescription,
                int groupId,
                int id,
                @Nullable Drawable startIcon,
                boolean isIconTintable,
                boolean groupContainsIcon,
                boolean enabled,
                @Nullable Intent intent,
                int order) {
            return mDelegate.getMenuItem(
                    title,
                    contentDescription,
                    groupId,
                    id,
                    startIcon,
                    isIconTintable,
                    groupContainsIcon,
                    enabled,
                    intent,
                    order);
        }

        @Override
        public long getNativeDelegate() {
            return mDelegate.getNativeDelegate();
        }
    }
}
