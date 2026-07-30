// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test

import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.test.junit4.v2.createEmptyComposeRule
import androidx.compose.ui.viewinterop.AndroidView
import androidx.test.filters.MediumTest
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.chromium.android_webview.test.util.CommonResources
import org.chromium.base.ThreadUtils
import org.chromium.base.test.util.Batch
import org.chromium.base.test.util.Feature
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper

/**
 * Reusable Composable wrapper for embedding AwTestContainerView.
 */
@Composable
fun WebViewHost(
    containerView: AwTestContainerView,
    modifier: Modifier = Modifier.fillMaxSize(),
    visible: Boolean = true,
) {
    if (visible) {
        AndroidView(
            modifier = modifier,
            factory = { containerView }
        )
    }
}

@RunWith(AwJUnit4ClassRunner::class)
@Batch(Batch.PER_CLASS)
class AwComposeTest {

    @get:Rule
    val mActivityTestRule = AwActivityTestRule()

    @get:Rule
    val mComposeTestRule = createEmptyComposeRule()

    private fun setComposeContent(content: @Composable () -> Unit) {
        ThreadUtils.runOnUiThreadBlocking {
            val composeView = ComposeView(mActivityTestRule.activity).apply { setContent(content) }
            mActivityTestRule.activity.addView(composeView)
        }
    }

    private fun loadPage(containerView: AwTestContainerView, helper: OnPageFinishedHelper) {
        mActivityTestRule.loadDataAsync(containerView.awContents, CommonResources.ABOUT_HTML, "text/html", false)
        helper.waitForNext()
    }

    @Test
    @MediumTest
    @Feature("AndroidWebView")
    fun testWebViewInCompose() {
        val client = TestAwContentsClient()
        val containerView = ThreadUtils.runOnUiThreadBlocking<AwTestContainerView> {
            mActivityTestRule.createDetachedAwTestContainerView(client)
        }

        setComposeContent {
            WebViewHost(containerView = containerView)
        }

        loadPage(containerView, client.onPageFinishedHelper)
    }

    @Test
    @MediumTest
    @Feature("AndroidWebView")
    fun testToggleWebViewVisibility() {
        val client = TestAwContentsClient()
        val containerView = ThreadUtils.runOnUiThreadBlocking<AwTestContainerView> {
            mActivityTestRule.createDetachedAwTestContainerView(client)
        }
        var showWebView by mutableStateOf(true)

        setComposeContent {
            WebViewHost(containerView = containerView, visible = showWebView)
        }

        loadPage(containerView, client.onPageFinishedHelper)

        // Hide WebView (detaches from window)
        ThreadUtils.runOnUiThreadBlocking { showWebView = false }
        mComposeTestRule.waitForIdle()

        // Show WebView (reattaches to window)
        ThreadUtils.runOnUiThreadBlocking { showWebView = true }
        mComposeTestRule.waitForIdle()

        loadPage(containerView, client.onPageFinishedHelper)
    }
}

