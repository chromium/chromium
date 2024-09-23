// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.notNullValue;

import android.content.Context;
import android.graphics.Rect;
import android.view.KeyEvent;
import android.view.ViewGroup;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;
import android.webkit.JavascriptInterface;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestInputMethodManagerWrapper;

/** Tests for IME (input method editor) on Android WebView. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwImeTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private static class TestJavascriptInterface {
        private final CallbackHelper mFocusCallbackHelper = new CallbackHelper();

        @JavascriptInterface
        public void onEditorFocused() {
            mFocusCallbackHelper.notifyCalled();
        }

        public CallbackHelper getFocusCallbackHelper() {
            return mFocusCallbackHelper;
        }
    }

    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private EditText mEditText;
    private final TestJavascriptInterface mTestJavascriptInterface = new TestJavascriptInterface();
    private TestInputMethodManagerWrapper mInputMethodManagerWrapper;

    public AwImeTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        mContentsClient = new TestAwContentsClient();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Use detached container view to avoid focus request.
                    mTestContainerView =
                            mActivityTestRule.createDetachedAwTestContainerView(mContentsClient);
                    mTestContainerView.setLayoutParams(
                            new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
                    mEditText = new EditText(mActivityTestRule.getActivity());
                    LinearLayout linearLayout = new LinearLayout(mActivityTestRule.getActivity());
                    linearLayout.setOrientation(LinearLayout.VERTICAL);
                    linearLayout.setLayoutParams(
                            new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

                    // Ensures that we don't autofocus EditText.
                    linearLayout.setDescendantFocusability(ViewGroup.FOCUS_AFTER_DESCENDANTS);
                    linearLayout.setFocusableInTouchMode(true);

                    mActivityTestRule.getActivity().addView(linearLayout);
                    linearLayout.addView(mEditText);
                    linearLayout.addView(mTestContainerView);
                    mTestContainerView
                            .getAwContents()
                            .addJavascriptInterface(mTestJavascriptInterface, "test");
                    // Let's not test against real input method.
                    ImeAdapter imeAdapter =
                            ImeAdapter.fromWebContents(mTestContainerView.getWebContents());
                    imeAdapter.setInputMethodManagerWrapper(
                            TestInputMethodManagerWrapper.create(imeAdapter));
                });
        AwActivityTestRule.enableJavaScriptOnUiThread(mTestContainerView.getAwContents());
    }

    private void loadContentEditableBody() throws Exception {
        final String mime = "text/html";
        final String htmlDocument = "<html><body contenteditable id='editor'></body></html>";
        final CallbackHelper loadHelper = mContentsClient.getOnPageFinishedHelper();

        mActivityTestRule.loadDataSync(
                mTestContainerView.getAwContents(), loadHelper, htmlDocument, mime, false);
    }

    private void loadBottomInputHtml() throws Throwable {
        // Shows an input at the bottom of the screen.
        final String htmlDocument =
                """
                        <html>
                        <head>
                            <style>
                                html,
                                body {
                                    background-color: beige
                                }

                                div {
                                    position: absolute;
                                    top: 10000px;
                                }
                            </style>
                        </head>

                        <body>Test<div id='footer'><input id='input_text'><br /></div>
                        </body>
                        </html>""";
        final CallbackHelper loadHelper = mContentsClient.getOnPageFinishedHelper();

        mActivityTestRule.loadHtmlSync(
                mTestContainerView.getAwContents(), loadHelper, htmlDocument);
    }

    private void focusOnEditTextAndShowKeyboard() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mEditText.requestFocus();
                    InputMethodManager imm =
                            (InputMethodManager)
                                    mActivityTestRule
                                            .getActivity()
                                            .getSystemService(Context.INPUT_METHOD_SERVICE);
                    imm.showSoftInput(mEditText, 0);
                });
    }

    private void focusOnWebViewAndEnableEditing() throws Exception {
        ThreadUtils.runOnUiThreadBlocking((Runnable) () -> mTestContainerView.requestFocus());

        // View focus may not have been propagated to the renderer process yet. If document is not
        // yet focused, and focusing on an element is an invalid operation. See crbug.com/622151
        // for details.
        mActivityTestRule.executeJavaScriptAndWaitForResult(
                mTestContainerView.getAwContents(),
                mContentsClient,
                """
                function onDocumentFocused() {
                        document.getElementById('editor').focus();
                        test.onEditorFocused();
                }
                (function() {
                if (document.hasFocus()) {
                        onDocumentFocused();
                } else {
                        window.addEventListener('focus', onDocumentFocused)
                }})();""");
        mTestJavascriptInterface.getFocusCallbackHelper().waitForCallback(0);
    }

    private InputConnection getInputConnection() {
        return ImeAdapter.fromWebContents(mTestContainerView.getWebContents())
                .getInputConnectionForTest();
    }

    private void waitForNonNullInputConnection() {
        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(getInputConnection(), notNullValue()));
    }

    /** Tests that moving from EditText to WebView keeps the keyboard showing. */
    // https://crbug.com/569556
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "TextInput"})
    public void testPressNextFromEditTextAndType() throws Throwable {
        loadContentEditableBody();
        focusOnEditTextAndShowKeyboard();
        focusOnWebViewAndEnableEditing();
        waitForNonNullInputConnection();
    }

    /**
     * Tests moving focus out of a WebView by calling InputConnection#sendKeyEvent() with a dpad
     * keydown event.
     */
    // https://crbug.com/787651
    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/795423")
    public void testImeDpadMovesFocusOutOfWebView() throws Throwable {
        loadContentEditableBody();
        focusOnEditTextAndShowKeyboard();
        focusOnWebViewAndEnableEditing();
        waitForNonNullInputConnection();

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mActivityTestRule.getActivity().getCurrentFocus(),
                            is(mTestContainerView));
                });

        ThreadUtils.runOnUiThreadBlocking(
                (Runnable)
                        () -> {
                            getInputConnection()
                                    .sendKeyEvent(
                                            new KeyEvent(
                                                    KeyEvent.ACTION_DOWN,
                                                    KeyEvent.KEYCODE_DPAD_UP));
                        });

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mActivityTestRule.getActivity().getCurrentFocus(), is(mEditText));
                });
    }

    /**
     * Tests moving focus out of a WebView by calling View#dispatchKeyEvent() with a dpad
     * keydown event.
     */
    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/795423")
    public void testDpadDispatchKeyEventMovesFocusOutOfWebView() throws Throwable {
        loadContentEditableBody();
        focusOnEditTextAndShowKeyboard();
        focusOnWebViewAndEnableEditing();
        waitForNonNullInputConnection();

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mActivityTestRule.getActivity().getCurrentFocus(),
                            is(mTestContainerView));
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTestContainerView.dispatchKeyEvent(
                            new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_UP));
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mActivityTestRule.getActivity().getCurrentFocus(), is(mEditText));
                });
    }

    private void scrollBottomOfNodeIntoView(String nodeId) throws Exception {
        mActivityTestRule.executeJavaScriptAndWaitForResult(
                mTestContainerView.getAwContents(),
                mContentsClient,
                "document.getElementById('" + nodeId + "').scrollIntoView(false);");
    }

    private float getScrollY() throws Exception {
        float res =
                Float.parseFloat(
                        mActivityTestRule.executeJavaScriptAndWaitForResult(
                                mTestContainerView.getAwContents(),
                                mContentsClient,
                                "window.scrollY"));
        return res;
    }

    // https://crbug.com/920061
    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1061218")
    public void testFocusAndViewSizeChangeCausesScroll() throws Throwable {
        loadBottomInputHtml();
        Rect currentRect = new Rect();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTestContainerView.getWindowVisibleDisplayFrame(currentRect);
                });
        WebContents webContents = mTestContainerView.getAwContents().getWebContents();

        DOMUtils.waitForNonZeroNodeBounds(webContents, "input_text");

        float initialScrollY = getScrollY();

        scrollBottomOfNodeIntoView("footer");
        // footer's offset is 10000 px from top. Scrolling this node into view should definitely
        // change the scroll.
        float scrollYAtBottom = getScrollY();
        assertThat(scrollYAtBottom, greaterThan(initialScrollY));

        DOMUtils.clickNode(
                webContents,
                "input_text",
                /* goThroughAndroidRootView= */ true,
                /* shouldScrollIntoView= */ false);

        CriteriaHelper.pollInstrumentationThread(
                () -> "input_text".equals(DOMUtils.getFocusedNode(webContents)));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Expect that we may have a size change.
                    ImeAdapter imeAdapter =
                            ImeAdapter.fromWebContents(mTestContainerView.getWebContents());
                    imeAdapter.onShowKeyboardReceiveResult(InputMethodManager.RESULT_SHOWN);

                    // When a virtual keyboard shows up, the window and view size shrink. Note that
                    // we are not depend on the real virtual keyboard behavior here. We only
                    // emulate the behavior by shrinking the view height.
                    currentRect.bottom = currentRect.centerY();
                    mTestContainerView.setWindowVisibleDisplayFrameOverride(currentRect);
                    int width = mTestContainerView.getWidth();
                    int height = mTestContainerView.getHeight();
                    mTestContainerView.onSizeChanged(width, height / 2, width, height);
                });

        // Scrolling may take some time. Ensures that scrolling happened.
        CriteriaHelper.pollInstrumentationThread(
                () -> (getScrollY() > scrollYAtBottom), "Scrolling should happen.");
    }
}
