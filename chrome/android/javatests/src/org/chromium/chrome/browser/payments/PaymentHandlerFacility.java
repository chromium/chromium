// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.widget.TextView;

import org.chromium.base.test.transit.ConditionStatusWithResult;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.base.test.transit.Element;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.payments.handler.PaymentHandlerCoordinator;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;

import java.util.concurrent.TimeoutException;

/**
 * The Expandable Payment Handler bottom sheet showing a fully loaded payment app page.
 *
 * <p>Waits for the page to finish loading, not only for its title. The title is set while the page
 * is still loading, and navigating away at that point aborts the load with ERR_ABORTED, which makes
 * PaymentHandlerMediator#didFailLoad() close the payment handler.
 *
 * <p>Not a BottomSheetFacility because back navigates the payment app instead of dismissing the
 * sheet.
 *
 * @param <HostStationT> the type of station this is scoped to.
 */
@NullMarked
public class PaymentHandlerFacility<HostStationT extends Station<ChromeTabbedActivity>>
        extends Facility<HostStationT> {
    private final PaymentHandlerCoordinator mPaymentHandler;
    private final String mExpectedTitle;
    private final String mExpectedOrigin;

    public final ViewElement<TextView> originElement;
    public final Element<WebContents> webContentsElement;

    public PaymentHandlerFacility(
            PaymentHandlerCoordinator paymentHandler, String expectedTitle, String expectedOrigin) {
        mPaymentHandler = paymentHandler;
        mExpectedTitle = expectedTitle;
        mExpectedOrigin = expectedOrigin;
        originElement =
                declareView(
                        declareView(withId(R.id.sheet_container))
                                .descendant(
                                        TextView.class,
                                        withId(R.id.origin),
                                        withText(expectedOrigin)));
        webContentsElement = declareEnterConditionAsElement(new PageLoadedCondition());
    }

    /** Fulfilled when the payment app page has the expected title and finished loading. */
    private class PageLoadedCondition extends ConditionWithResult<WebContents> {
        PageLoadedCondition() {
            super(/* isRunOnUiThread= */ true);
        }

        @Override
        protected ConditionStatusWithResult<WebContents> resolveWithSuppliers() {
            WebContents webContents = mPaymentHandler.getWebContentsForTest();
            if (webContents == null || webContents.isDestroyed()) {
                return notFulfilled("No payment app WebContents").withoutResult();
            }
            String title = webContents.getTitle();
            boolean loading = webContents.isLoading();
            return whether(
                            mExpectedTitle.equals(title) && !loading,
                            "title \"%s\", isLoading %b",
                            title,
                            loading)
                    .withResult(webContents);
        }

        @Override
        public String buildDescription() {
            return String.format("Payment app page \"%s\" loaded", mExpectedTitle);
        }
    }

    private PaymentHandlerFacility<HostStationT> newWithTitle(String expectedTitle) {
        return new PaymentHandlerFacility<>(mPaymentHandler, expectedTitle, mExpectedOrigin);
    }

    /** Navigates the payment app to |url|, expecting a page titled |expectedTitle|. */
    public PaymentHandlerFacility<HostStationT> loadUrl(String url, String expectedTitle) {
        return runOnUiThreadTo(
                        () ->
                                webContentsElement
                                        .value()
                                        .getNavigationController()
                                        .loadUrl(new LoadUrlParams(url)))
                .exitFacilityAnd()
                .enterFacility(newWithTitle(expectedTitle));
    }

    /**
     * Executes |script| in the payment app page and waits for it to finish, expecting a page titled
     * |expectedTitle|.
     */
    public PaymentHandlerFacility<HostStationT> executeJavaScript(
            String script, String expectedTitle) {
        return runTo(
                        () -> {
                            try {
                                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                        webContentsElement.value(), script);
                            } catch (TimeoutException e) {
                                throw new RuntimeException(e);
                            }
                        })
                .exitFacilityAnd()
                .enterFacility(newWithTitle(expectedTitle));
    }

    /**
     * Presses back, expecting the payment app to navigate back to a page titled |expectedTitle|.
     */
    public PaymentHandlerFacility<HostStationT> pressBack(String expectedTitle) {
        return pressBackTo().exitFacilityAnd().enterFacility(newWithTitle(expectedTitle));
    }

    /** Presses back when the payment app has no previous page, which is a no-op. */
    public PaymentHandlerFacility<HostStationT> pressBackWithNoHistory() {
        return pressBackTo()
                .withPossiblyAlreadyFulfilled()
                .exitFacilityAnd()
                .enterFacility(newWithTitle(mExpectedTitle));
    }

    /** Hides the payment handler. */
    public void hide() {
        runOnUiThreadTo(mPaymentHandler::hide).exitFacility();
    }
}
