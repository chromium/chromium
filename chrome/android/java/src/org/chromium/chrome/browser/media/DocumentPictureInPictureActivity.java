// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.annotation.SuppressLint;
import android.content.Intent;
import android.view.ViewGroup;

import org.jni_zero.NativeMethods;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.url.GURL;

@NullMarked
public class DocumentPictureInPictureActivity extends AsyncInitializationActivity {
    public static final String WEB_CONTENTS_KEY =
            "org.chromium.chrome.browser.media.DocumentPictureInPicture.WebContents";
    private WebContents mWebContents;
    private Tab mInitiatorTab;
    private @Nullable ThinWebView mThinWebView;

    @Override
    protected void onPreCreate() {
        super.onPreCreate();

        Intent intent = getIntent();
        intent.setExtrasClassLoader(WebContents.class.getClassLoader());
        WebContents webContents = intent.getParcelableExtra(WEB_CONTENTS_KEY);
        if (webContents == null) {
            finish();
            return;
        }

        mWebContents = webContents;
        WebContents parentWebContents = mWebContents.getDocumentPictureInPictureOpener();
        mInitiatorTab = TabUtils.fromWebContents(parentWebContents);
        if (parentWebContents == null || TabUtils.getActivity(mInitiatorTab) == null) {
            finish();
            return;
        }
    }

    /**
     * @return Whether the document pip WebContents and the initiator tab are both initialized.
     */
    @EnsuresNonNullIf({"mWebContents", "mInitiatorTab"})
    private boolean isContentsInitialized() {
        return mWebContents != null && mInitiatorTab != null;
    }

    @Override
    @SuppressLint("NewAPI")
    public void onStart() {
        super.onStart();
        assert isContentsInitialized();

        if (mInitiatorTab.getWebContents() == null) {
            finish();
            return;
        }

        DocumentPictureInPictureActivityJni.get()
                .onActivityStart(mInitiatorTab.getWebContents(), mWebContents);
    }

    @Override
    public void initializeCompositor() {
        ActivityWindowAndroid windowAndroid = getWindowAndroid();
        if (windowAndroid == null) {
            windowAndroid = createWindowAndroid();
        }
        ContentView contentView = ContentView.createContentView(this, mWebContents);
        mThinWebView = ThinWebViewFactory.create(this, new ThinWebViewConstraints(), windowAndroid);
        mThinWebView.attachWebContents(
                mWebContents, contentView, new DocumentPictureInPictureWebContentsDelegate());
        mWebContents.setDelegates(
                VersionInfo.getProductVersion(),
                ViewAndroidDelegate.createBasicDelegate(contentView),
                contentView,
                windowAndroid,
                WebContents.createDefaultInternalsHolder());

        addContentView(
                mThinWebView.getView(),
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
    }

    @Override
    protected void triggerLayoutInflation() {
        assert isContentsInitialized();
        onInitialLayoutInflationComplete();
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return true;
    }

    @Override
    protected OneshotSupplier<ProfileProvider> createProfileProvider() {
        OneshotSupplierImpl<ProfileProvider> supplier = new OneshotSupplierImpl<>();
        ProfileProvider profileProvider =
                new ProfileProvider() {

                    @Override
                    public Profile getOriginalProfile() {
                        return mInitiatorTab.getProfile().getOriginalProfile();
                    }

                    @Override
                    public @Nullable Profile getOffTheRecordProfile(boolean createIfNeeded) {
                        if (!mInitiatorTab.getProfile().isOffTheRecord()) {
                            assert !createIfNeeded;
                            return null;
                        }
                        return mInitiatorTab.getProfile();
                    }
                };
        supplier.set(profileProvider);
        return supplier;
    }

    @Override
    protected ActivityWindowAndroid createWindowAndroid() {
        return new ActivityWindowAndroid(
                this,
                /* listenToActivityState= */ true,
                getIntentRequestTracker(),
                getInsetObserver(),
                /* trackOcclusion= */ true);
    }

    @Override
    @SuppressWarnings("NullAway")
    protected final void onDestroy() {
        if (mThinWebView != null) {
            mThinWebView.destroy();
            mThinWebView = null;
        }
        if (mWebContents != null) {
            mWebContents.destroy();
            mWebContents = null;
        }

        super.onDestroy();
    }

    private class DocumentPictureInPictureWebContentsDelegate extends WebContentsDelegateAndroid {
        @Override
        public void closeContents() {
            finish();
        }

        @Override
        public void openNewTab(
                GURL url,
                String extraHeaders,
                ResourceRequestBody postData,
                int disposition,
                boolean isRendererInitiated) {
            finish();
        }
    }

    @NativeMethods
    public interface Natives {
        void onActivityStart(WebContents parentWebContent, WebContents webContents);
    }
}
