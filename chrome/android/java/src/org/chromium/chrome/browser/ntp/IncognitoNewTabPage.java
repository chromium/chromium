// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.app.Activity;
import android.graphics.Canvas;
import android.view.LayoutInflater;

import androidx.core.view.ViewCompat;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.IncognitoNewTabPageView.IncognitoNewTabPageManager;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_ui.InvalidationAwareThumbnailProvider;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.user_prefs.UserPrefs;

/** Provides functionality when the user interacts with the Incognito NTP. */
public class IncognitoNewTabPage extends BasicNativePage
        implements InvalidationAwareThumbnailProvider {

    private static IncognitoNewTabPageManager sIncognitoNtpManagerForTesting;
    private final Activity mActivity;
    private final Profile mProfile;
    private final int mIncognitoNtpBackgroundColor;

    private String mTitle;
    protected IncognitoNewTabPageView mIncognitoNewTabPageView;

    private boolean mIsLoaded;

    private IncognitoNewTabPageManager mIncognitoNewTabPageManager;
    private IncognitoCookieControlsManager mCookieControlsManager;
    private IncognitoCookieControlsManager.Observer mCookieControlsObserver;
    private EdgeToEdgePadAdjuster mEdgeToEdgePadAdjuster;

    private void showIncognitoLearnMore() {
        HelpAndFeedbackLauncherImpl.getForProfile(mProfile)
                .show(
                        mActivity,
                        mActivity.getString(R.string.help_context_incognito_learn_more),
                        null);
    }

    /**
     * Constructs an Incognito NewTabPage.
     *
     * @param activity The activity used to create the new tab page's View.
     * @param host The view that's hosting this incognito NTP.
     * @param profile The profile associated with this incognito NTP.
     * @param edgeToEdgeControllerSupplier The supplier for e2e status and the bottom inset.
     */
    public IncognitoNewTabPage(
            Activity activity,
            NativePageHost host,
            Profile profile,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier) {
        super(host);

        mActivity = activity;
        mProfile = profile;

        if (!mProfile.isOffTheRecord()) {
            throw new IllegalStateException(
                    "Attempting to create an incognito NTP with a normal profile.");
        }

        mIncognitoNtpBackgroundColor = host.getContext().getColor(R.color.ntp_bg_incognito);

        mIncognitoNewTabPageManager = createIncognitoNewTabPageManager();

        mTitle = host.getContext().getResources().getString(R.string.new_incognito_tab_title);

        LayoutInflater inflater = LayoutInflater.from(host.getContext());
        mIncognitoNewTabPageView =
                (IncognitoNewTabPageView) inflater.inflate(R.layout.new_tab_page_incognito, null);
        mIncognitoNewTabPageView.initialize(mIncognitoNewTabPageManager);

        // Work around https://crbug.com/943873 and https://crbug.com/963385 where default focus
        // highlight shows up after toggling dark mode.
        mIncognitoNewTabPageView.setDefaultFocusHighlightEnabled(false);

        initWithView(mIncognitoNewTabPageView);

        if (EdgeToEdgeUtils.isDrawKeyNativePageToEdgeEnabled()) {
            mEdgeToEdgePadAdjuster =
                    EdgeToEdgeControllerFactory.createForViewAndObserveSupplier(
                            mIncognitoNewTabPageView.getScrollView(), edgeToEdgeControllerSupplier);
        }
    }

    /**
     * @return Whether the NTP has finished loaded.
     */
    public boolean isLoadedForTests() {
        return mIsLoaded;
    }

    // NativePage overrides

    @Override
    public void destroy() {
        assert !ViewCompat.isAttachedToWindow(getView())
                : "Destroy called before removed from window";
        mIncognitoNewTabPageManager.destroy();

        if (mEdgeToEdgePadAdjuster != null) {
            mEdgeToEdgePadAdjuster.destroy();
            mEdgeToEdgePadAdjuster = null;
        }

        super.destroy();
    }

    @Override
    public String getUrl() {
        return UrlConstants.NTP_URL;
    }

    @Override
    public int getBackgroundColor() {
        return mIncognitoNtpBackgroundColor;
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public boolean needsToolbarShadow() {
        return true;
    }

    @Override
    public String getHost() {
        return UrlConstants.NTP_HOST;
    }

    @Override
    public void updateForUrl(String url) {}

    @Override
    public boolean supportsEdgeToEdge() {
        return !EdgeToEdgeUtils.DISABLE_INCOGNITO_NTP_E2E.getValue();
    }

    // InvalidationAwareThumbnailProvider

    @Override
    public boolean shouldCaptureThumbnail() {
        return mIncognitoNewTabPageView.shouldCaptureThumbnail();
    }

    @Override
    public void captureThumbnail(Canvas canvas) {
        mIncognitoNewTabPageView.captureThumbnail(canvas);
    }

    private IncognitoNewTabPageManager createIncognitoNewTabPageManager() {
        if (sIncognitoNtpManagerForTesting != null) return sIncognitoNtpManagerForTesting;

        return new IncognitoNewTabPageManager() {
            @Override
            public void loadIncognitoLearnMore() {
                showIncognitoLearnMore();
            }

            @Override
            public void initCookieControlsManager() {
                mCookieControlsManager = new IncognitoCookieControlsManager();
                mCookieControlsManager.initialize(mProfile);
                mCookieControlsObserver =
                        new IncognitoCookieControlsManager.Observer() {
                            @Override
                            public void onUpdate(
                                    boolean checked, @CookieControlsEnforcement int enforcement) {
                                mIncognitoNewTabPageView
                                        .setIncognitoCookieControlsToggleEnforcement(enforcement);
                                mIncognitoNewTabPageView.setIncognitoCookieControlsToggleChecked(
                                        checked);
                            }
                        };
                mCookieControlsManager.addObserver(mCookieControlsObserver);
                mIncognitoNewTabPageView.setIncognitoCookieControlsToggleCheckedListener(
                        mCookieControlsManager);
                mIncognitoNewTabPageView.setIncognitoCookieControlsIconOnclickListener(
                        mCookieControlsManager);
                mCookieControlsManager.updateIfNecessary();
            }

            @Override
            public boolean shouldCaptureThumbnail() {
                return mCookieControlsManager.shouldCaptureThumbnail();
            }

            @Override
            public boolean shouldShowTrackingProtectionNtp() {
                return UserPrefs.get(mProfile).getBoolean(Pref.TRACKING_PROTECTION3PCD_ENABLED)
                        || ChromeFeatureList.isEnabled(ChromeFeatureList.TRACKING_PROTECTION_3PCD);
            }

            @Override
            public void destroy() {
                if (mCookieControlsManager != null) {
                    mCookieControlsManager.removeObserver(mCookieControlsObserver);
                    mCookieControlsManager.destroy();
                }
            }

            @Override
            public void onLoadingComplete() {
                mIsLoaded = true;
            }
        };
    }

    /** Set a stubbed {@link IncognitoNewTabPageManager} for testing. */
    public static void setIncognitoNtpManagerForTesting(IncognitoNewTabPageManager manager) {
        sIncognitoNtpManagerForTesting = manager;
        ResettersForTesting.register(() -> sIncognitoNtpManagerForTesting = null);
    }
}
