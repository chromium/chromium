// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.link_to_text;

import android.net.Uri;

import org.chromium.base.Callback;
import org.chromium.blink.mojom.TextFragmentReceiver;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** This class provides the utility methods for link to text. */
public class LinkToTextHelper {
    public static final String SHARED_HIGHLIGHTING_SUPPORT_URL =
            "https://support.google.com/chrome?p=shared_highlighting";
    public static final String TEXT_FRAGMENT_PREFIX = ":~:text=";
    public static final String ADDITIONAL_TEXT_FRAGMENT_SELECTOR = "&text=";

    interface RequestSelectorCallback {
        void apply(String selector, Integer errorCode, Integer readyStatus);
    }

    /**
     * Removes highlights from all frames in the page.
     *
     * @param webContents The webContents to get all <link RenderFrameHost> in the current page.
     */
    public static void removeHighlightsAllFrames(WebContents webContents) {
        List<RenderFrameHost> renderFrameHosts =
                webContents.getMainFrame().getAllRenderFrameHosts();

        for (RenderFrameHost renderFrameHost : renderFrameHosts) {
            TextFragmentReceiver producer =
                    renderFrameHost.getInterfaceToRendererFrame(TextFragmentReceiver.MANAGER);
            producer.removeFragments();
        }
    }

    /**
     * Fetch the text fragment matches (highlighted text) for the current frame.
     *
     * @param producer The {@link TextFragmentReceiver} to make the renderer call for the current
     *         frame.
     * @param callback The {@link Callback} to handle the text fragment matches result.
     */
    public static void extractTextFragmentsMatches(
            TextFragmentReceiver producer, Callback<String[]> callback) {
        producer.extractTextFragmentsMatches(
                new TextFragmentReceiver.ExtractTextFragmentsMatches_Response() {
                    @Override
                    public void call(String[] matches) {
                        callback.onResult(matches);
                    }
                });
    }

    /**
     * Fetch the canonical url for sharing
     *
     * @param tab The tab to fetch the canonical url from.
     * @param callback The {@link Callback} to return the tab's canonical url or an empty string
     */
    public static void requestCanonicalUrl(Tab tab, Callback<String> callback) {
        if (!shouldRequestCanonicalUrl(tab)) {
            callback.onResult("");
            return;
        }

        tab.getWebContents()
                .getMainFrame()
                .getCanonicalUrlForSharing(
                        new Callback<GURL>() {
                            @Override
                            public void onResult(GURL result) {
                                callback.onResult(result.getSpec());
                            }
                        });
    }

    private static boolean shouldRequestCanonicalUrl(Tab tab) {
        if (tab.getWebContents() == null) return false;
        if (tab.getWebContents().getMainFrame() == null) return false;
        if (tab.getUrl().isEmpty()) return false;
        if (tab.isShowingErrorPage() || SadTab.isShowing(tab)) {
            return false;
        }
        return true;
    }

    /**
     * This checks if there is a highlight in the page by iterating over the frames in the page to
     * find an existing selectors.
     *
     * @param tab The tab to get all <link RenderFrameHost> in the current page.
     * @param callback The {@link Callback} to handle whether or not there is a highlight on the
     *         current page.
     */
    public static void hasExistingSelectors(Tab tab, Callback<Boolean> callback) {
        List<RenderFrameHost> renderFrameHosts =
                tab.getWebContents().getMainFrame().getAllRenderFrameHosts();

        for (RenderFrameHost renderFrameHost : renderFrameHosts) {
            TextFragmentReceiver producer =
                    renderFrameHost.getInterfaceToRendererFrame(TextFragmentReceiver.MANAGER);
            if (producer == null) {
                continue;
            }

            getExistingSelectorsForFrame(
                    producer,
                    (text) -> {
                        if (text.length > 0) {
                            callback.onResult(true);
                        } else {
                            callback.onResult(false);
                        }
                        producer.close();
                    });
        }
    }

    /**
     * Returns the url to share by encoding the text fragment selector to the URL.
     *
     * @param url The url that will encode the text fragment selector.
     * @param selector The text fragment selector that will be encoded.
     */
    public static String getUrlToShare(String url, String selector) {
        if (!selector.isEmpty()) {
            Uri uri = Uri.parse(url);
            url = uri.buildUpon().encodedFragment(TEXT_FRAGMENT_PREFIX + selector).toString();
        }
        return url;
    }

    /**
     * Fetch the existing selectors from all the frames in the current page by recursively iterating
     * over the frame tree.
     *
     * @param tab The tab to get all <link RenderFrameHost> in the current page.
     * @param callback The {@link Callback} to handle the existing selectors result.
     */
    public static void getExistingSelectorsAllFrames(Tab tab, Callback<String> callback) {
        List<RenderFrameHost> renderFrameHosts =
                tab.getWebContents().getMainFrame().getAllRenderFrameHosts();
        getExistingSelectorsFromFrameAtIndex(
                new ArrayList<String>(), renderFrameHosts, callback, /* index= */ 0);
    }

    /**
     * Fetch recursively existing selectors from {@link RenderFrameHost} at index and add results to
     * the list of selectors.
     *
     * @param selectorsList The list of existing selectors in all frames.
     * @param renderFrameHosts The list of all {@link RenderFrameHost} from the current page.
     * @param callback The {@link Callback} to handle the existing selectors result.
     * @param index The index of the item in {@link List<RenderFrameHost>}
     */
    private static void getExistingSelectorsFromFrameAtIndex(
            List<String> selectorsList,
            List<RenderFrameHost> renderFrameHosts,
            Callback<String> callback,
            int index) {
        if (index >= renderFrameHosts.size()) {
            String selectors = String.join(ADDITIONAL_TEXT_FRAGMENT_SELECTOR, selectorsList);
            callback.onResult(selectors);
            return;
        }

        RenderFrameHost renderFrameHost = renderFrameHosts.get(index);
        TextFragmentReceiver producer =
                renderFrameHost.getInterfaceToRendererFrame(TextFragmentReceiver.MANAGER);

        if (producer == null) {
            getExistingSelectorsFromFrameAtIndex(
                    selectorsList, renderFrameHosts, callback, index + 1);
            return;
        }

        getExistingSelectorsForFrame(
                producer,
                (selectors) -> {
                    if (selectors.length > 0) {
                        selectorsList.addAll(Arrays.asList(selectors));
                    }
                    getExistingSelectorsFromFrameAtIndex(
                            selectorsList, renderFrameHosts, callback, index + 1);
                    producer.close();
                });
    }

    /**
     * Fetch the existing selectors in the current frame.
     *
     * @param producer The {@link TextFragmentReceiver} to make the renderer call for the current
     *         frame.
     * @param callback The {@link Callback} to handle the existing selectors result.
     */
    public static void getExistingSelectorsForFrame(
            TextFragmentReceiver producer, Callback<String[]> callback) {
        producer.getExistingSelectors(
                new TextFragmentReceiver.GetExistingSelectors_Response() {
                    @Override
                    public void call(String[] text) {
                        callback.onResult(text);
                    }
                });
    }

    /**
     * Fetch the generated selector that uniquely identify the highlighted text selected text.
     *
     * @param producer The {@link TextFragmentReceiver} to make the renderer call for the current
     *         frame.
     * @param callback The {@link Callback} to handle the generated selector.
     */
    public static void requestSelector(
            TextFragmentReceiver producer, RequestSelectorCallback callback) {
        producer.requestSelector(
                (String selector, int error, int readyStatus) -> {
                    LinkToTextMetricsHelper.recordLinkToTextDiagnoseStatus(
                            LinkToTextMetricsHelper.LinkToTextDiagnoseStatus.SELECTOR_RECEIVED);
                    callback.apply(selector, error, readyStatus);
                });
    }

    /**
     * This checks that the URL has a text fragment selector (e.g: #:~:text=selector) attached.
     *
     * @param url The url which may include the text fragment.
     * @return whether or not the url had a text fragment selector.
     */
    public static boolean hasTextFragment(GURL url) {
        Uri uri = Uri.parse(url.getSpec());
        String fragment = uri.getEncodedFragment();
        return (fragment != null) && fragment.contains(TEXT_FRAGMENT_PREFIX);
    }
}
