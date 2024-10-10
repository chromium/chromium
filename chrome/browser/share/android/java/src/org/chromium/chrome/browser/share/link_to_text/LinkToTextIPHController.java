// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.link_to_text;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageScopeType;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** This class is responsible for rendering an IPH, when receiving a link-to-text. */
public class LinkToTextIPHController {
    private static final String FEATURE_NAME =
            FeatureConstants.SHARED_HIGHLIGHTING_RECEIVER_FEATURE;

    private final TabModelSelector mTabModelSelector;
    private Tracker mTracker;

    /**
     * Creates an {@link LinkToTextIPHController}.
     *
     * @param tabSupplier An {@link ObservableSupplier} for {@link Tab} where the IPH will be
     *     rendered.
     * @param tabModelSelector The {@link TabModelSelector} to open a new tab.
     */
    public LinkToTextIPHController(
            ObservableSupplier<Tab> tabSupplier,
            TabModelSelector tabModelSelector,
            ObservableSupplier<Profile> profileSupplier) {
        mTabModelSelector = tabModelSelector;
        new CurrentTabObserver(
                tabSupplier,
                new EmptyTabObserver() {
                    @Override
                    public void onPageLoadFinished(Tab tab, GURL url) {
                        if (!LinkToTextHelper.hasTextFragment(url)) return;

                        Profile profile = profileSupplier.get();
                        if (profile == null) {
                            assert false : "Unexpected null profile";
                            return;
                        }
                        mTracker = TrackerFactory.getTrackerForProfile(profile);
                        if (!mTracker.wouldTriggerHelpUI(FEATURE_NAME)) {
                            return;
                        }

                        LinkToTextHelper.hasExistingSelectors(
                                tab,
                                (hasSelectors) -> {
                                    if (mTracker.shouldTriggerHelpUI(FEATURE_NAME)) {
                                        showMessageIPH(tab);
                                    }
                                });
                    }
                },
                null);
    }

    private void showMessageIPH(Tab tab) {
        MessageDispatcher mMessageDispatcher =
                MessageDispatcherProvider.from(tab.getWindowAndroid());
        PropertyModel model =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.SHARED_HIGHLIGHTING)
                        .with(
                                MessageBannerProperties.ICON,
                                TraceEventVectorDrawableCompat.create(
                                        tab.getContext().getResources(),
                                        R.drawable.ink_highlighter,
                                        tab.getContext().getTheme()))
                        .with(
                                MessageBannerProperties.TITLE,
                                tab.getContext()
                                        .getResources()
                                        .getString(R.string.iph_message_shared_highlighting_title))
                        .with(
                                MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                tab.getContext()
                                        .getResources()
                                        .getString(R.string.iph_message_shared_highlighting_button))
                        .with(MessageBannerProperties.ON_DISMISSED, this::onMessageDismissed)
                        .with(
                                MessageBannerProperties.ON_PRIMARY_ACTION,
                                this::onMessageButtonClicked)
                        .build();
        mMessageDispatcher.enqueueMessage(
                model, tab.getWebContents(), MessageScopeType.NAVIGATION, false);
    }

    private @PrimaryActionClickBehavior int onMessageButtonClicked() {
        onOpenInChrome(LinkToTextHelper.SHARED_HIGHLIGHTING_SUPPORT_URL);
        mTracker.dismissed(FEATURE_NAME);
        return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
    }

    private void onMessageDismissed(Integer dismissReason) {
        mTracker.dismissed(FEATURE_NAME);
    }

    private void onOpenInChrome(String linkUrl) {
        mTabModelSelector.openNewTab(
                new LoadUrlParams(linkUrl),
                TabLaunchType.FROM_LINK,
                mTabModelSelector.getCurrentTab(),
                mTabModelSelector.isIncognitoSelected());
    }
}
