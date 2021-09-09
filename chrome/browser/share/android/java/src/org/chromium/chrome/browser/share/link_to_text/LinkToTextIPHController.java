// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.link_to_text;

import android.net.Uri;

import androidx.vectordrawable.graphics.drawable.VectorDrawableCompat;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.blink.mojom.TextFragmentReceiver;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageScopeType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/**
 * This class is responsible for rendering an IPH, when receiving a link-to-text.
 */
public class LinkToTextIPHController {
    private final TabModelSelector mTabModelSelector;
    private CurrentTabObserver mCurrentTabObserver;

    /**
     * Creates an {@link LinkToTextIPHController}.
     * @param Tab The {@link Tab} where the IPH will be rendered.
     */
    public LinkToTextIPHController(
            ObservableSupplier<Tab> tabSupplier, TabModelSelector tabModelSelector) {
        // TODO(cheickcisse): Create this object in {@link TabbedRootUiCoordinator} and check
        // feature tracker.
        mTabModelSelector = tabModelSelector;
        mCurrentTabObserver = new CurrentTabObserver(tabSupplier, new EmptyTabObserver() {
            @Override
            public void onPageLoadFinished(Tab tab, GURL url) {
                if (!hasTextFragment(tab, url)) return;
                getExistingSelectors(tab);
            }
        }, null);
    }

    // Request text fragment selectors for existing highlights
    private void getExistingSelectors(Tab tab) {
        TextFragmentReceiver producer =
                tab.getWebContents().getMainFrame().getInterfaceToRendererFrame(
                        TextFragmentReceiver.MANAGER);
        LinkToTextCoordinator.getExistingSelectors(producer, (text) -> {
            if (text.length > 0) {
                showMessageIPH(tab);
            }
            producer.close();
        });
    }

    private void showMessageIPH(Tab tab) {
        MessageDispatcher mMessageDispatcher =
                MessageDispatcherProvider.from(tab.getWindowAndroid());
        PropertyModel model =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.SHARED_HIGHLIGHTING)
                        .with(MessageBannerProperties.ICON,
                                VectorDrawableCompat.create(tab.getContext().getResources(),
                                        R.drawable.ink_highlighter, tab.getContext().getTheme()))
                        .with(MessageBannerProperties.TITLE,
                                tab.getContext().getResources().getString(
                                        R.string.iph_message_shared_highlighting_title))
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                tab.getContext().getResources().getString(
                                        R.string.iph_message_shared_highlighting_button))
                        .with(MessageBannerProperties.ON_DISMISSED, this::onMessageDismissed)
                        .with(MessageBannerProperties.ON_PRIMARY_ACTION,
                                this::onMessageButtonClicked)
                        .build();
        mMessageDispatcher.enqueueMessage(
                model, tab.getWebContents(), MessageScopeType.NAVIGATION, false);
    }

    private void onMessageButtonClicked() {
        onOpenInChrome(LinkToTextCoordinator.SHARED_HIGHLIGHTING_SUPPORT_URL);
    }

    private void onMessageDismissed(Integer dismissReason) {}

    private void onOpenInChrome(String linkUrl) {
        mTabModelSelector.openNewTab(new LoadUrlParams(linkUrl), TabLaunchType.FROM_LINK,
                mTabModelSelector.getCurrentTab(), mTabModelSelector.isIncognitoSelected());
    }

    private boolean hasTextFragment(Tab tab, GURL url) {
        Uri uri = Uri.parse(url.getSpec());
        String fragment = uri.getEncodedFragment();
        return fragment != null ? fragment.contains(LinkToTextCoordinator.TEXT_FRAGMENT_PREFIX)
                                : false;
    }
}
