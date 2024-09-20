// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Parcelable;
import android.text.TextUtils;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.payments.Address;
import org.chromium.components.payments.ErrorStrings;
import org.chromium.components.payments.IPaymentDetailsUpdateService;
import org.chromium.components.payments.IPaymentDetailsUpdateServiceCallback;
import org.chromium.components.payments.PaymentDetailsUpdateService;
import org.chromium.components.payments.PaymentDetailsUpdateServiceHelper;
import org.chromium.components.payments.PaymentRequestUpdateEventListener;
import org.chromium.components.payments.intent.WebPaymentIntentHelperType.PaymentCurrencyAmount;
import org.chromium.components.payments.intent.WebPaymentIntentHelperType.PaymentHandlerMethodData;
import org.chromium.components.payments.intent.WebPaymentIntentHelperType.PaymentRequestDetailsUpdate;
import org.chromium.components.payments.intent.WebPaymentIntentHelperType.PaymentShippingOption;
import org.chromium.payments.mojom.PaymentAddress;

import java.util.ArrayList;
import java.util.List;

/** Tests for PaymentDetailsUpdateServiceHelper. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentDetailsUpdateServiceHelperTest {
    private static final int DECODER_STARTUP_TIMEOUT_IN_MS = 10000;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public ExpectedException thrown = ExpectedException.none();

    /** Simulates a package manager in memory. */
    private final MockPackageManagerDelegate mPackageManager = new MockPackageManagerDelegate();

    private Context mContext;

    private Bundle defaultAddressBundle() {
        Bundle bundle = new Bundle();
        bundle.putString(Address.EXTRA_ADDRESS_COUNTRY, "CA");
        String[] addressLine = {"111 Richmond Street West"};
        bundle.putStringArray(Address.EXTRA_ADDRESS_LINES, addressLine);
        bundle.putString(Address.EXTRA_ADDRESS_REGION, "Ontario");
        bundle.putString(Address.EXTRA_ADDRESS_CITY, "Toronto");
        bundle.putString(Address.EXTRA_ADDRESS_POSTAL_CODE, "M5H2G4");
        bundle.putString(Address.EXTRA_ADDRESS_RECIPIENT, "John Smith");
        bundle.putString(Address.EXTRA_ADDRESS_PHONE, "4169158200");
        return bundle;
    }

    private Bundle defaultMethodDataBundle() {
        Bundle bundle = new Bundle();
        bundle.putString(PaymentHandlerMethodData.EXTRA_METHOD_NAME, "method-name");
        bundle.putString(
                PaymentHandlerMethodData.EXTRA_STRINGIFIED_DETAILS, "{\"key\": \"value\"}");
        return bundle;
    }

    private boolean mBound;
    private IPaymentDetailsUpdateService mIPaymentDetailsUpdateService;
    private ServiceConnection mConnection =
            new ServiceConnection() {
                @Override
                public void onServiceConnected(ComponentName className, IBinder service) {
                    mIPaymentDetailsUpdateService =
                            IPaymentDetailsUpdateService.Stub.asInterface(service);
                    mBound = true;
                }

                @Override
                public void onServiceDisconnected(ComponentName className) {
                    mIPaymentDetailsUpdateService = null;
                    mBound = false;
                }
            };

    @Before
    public void setUp() throws Throwable {
        mActivityTestRule.startMainActivityOnBlankPage();
        mContext = mActivityTestRule.getActivity();
    }

    private void installPaymentApp() {
        mPackageManager.installPaymentApp(
                "BobPay",
                /* packageName= */ "com.bobpay",
                null /* no metadata */,
                /* signature= */ "01");

        mPackageManager.setInvokedAppPackageName(/* packageName= */ "com.bobpay");
    }

    private void installAndInvokePaymentApp() throws Throwable {
        installPaymentApp();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PaymentDetailsUpdateServiceHelper.getInstance()
                            .initialize(
                                    mPackageManager,
                                    /* packageName= */ "com.bobpay",
                                    mUpdateListener);
                });
    }

    private void updateWithDefaultDetails() throws Throwable {
        PaymentCurrencyAmount total =
                new PaymentCurrencyAmount(/* currency= */ "CAD", /* value= */ "10.00");

        // Populate shipping options.
        List<PaymentShippingOption> shippingOptions = new ArrayList<PaymentShippingOption>();
        shippingOptions.add(
                new PaymentShippingOption(
                        "shippingId",
                        "Free shipping",
                        new PaymentCurrencyAmount("CAD", "0.00"),
                        /* selected= */ true));

        // Populate address errors.
        Bundle bundledShippingAddressErrors = new Bundle();
        bundledShippingAddressErrors.putString("addressLine", "invalid address line");
        bundledShippingAddressErrors.putString("city", "invalid city");
        bundledShippingAddressErrors.putString("countryCode", "invalid country code");
        bundledShippingAddressErrors.putString("dependentLocality", "invalid dependent locality");
        bundledShippingAddressErrors.putString("organization", "invalid organization");
        bundledShippingAddressErrors.putString("phone", "invalid phone");
        bundledShippingAddressErrors.putString("postalCode", "invalid postal code");
        bundledShippingAddressErrors.putString("recipient", "invalid recipient");
        bundledShippingAddressErrors.putString("region", "invalid region");
        bundledShippingAddressErrors.putString("sortingCode", "invalid sorting code");

        PaymentRequestDetailsUpdate response =
                new PaymentRequestDetailsUpdate(
                        total,
                        shippingOptions,
                        /* error= */ "error message",
                        /* pstringifiedPaymentMethodErrors= */ "stringified payment method",
                        bundledShippingAddressErrors);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PaymentDetailsUpdateServiceHelper.getInstance().updateWith(response);
                });
    }

    private void onPaymentDetailsNotUpdated() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PaymentDetailsUpdateServiceHelper.getInstance().onPaymentDetailsNotUpdated();
                });
    }

    private void verifyUpdatedDefaultDetails() {
        Bundle total = mUpdatedPaymentDetails.getBundle(PaymentRequestDetailsUpdate.EXTRA_TOTAL);
        Assert.assertEquals("CAD", total.getString(PaymentCurrencyAmount.EXTRA_CURRENCY));
        Assert.assertEquals("10.00", total.getString(PaymentCurrencyAmount.EXTRA_VALUE));

        // Validate shipping options
        Parcelable[] shippingOptions =
                mUpdatedPaymentDetails.getParcelableArray(
                        PaymentRequestDetailsUpdate.EXTRA_SHIPPING_OPTIONS);
        Assert.assertEquals(1, shippingOptions.length);
        Bundle shippingOption = (Bundle) shippingOptions[0];
        Assert.assertEquals(
                "shippingId",
                shippingOption.getString(PaymentShippingOption.EXTRA_SHIPPING_OPTION_ID));
        Assert.assertEquals(
                "Free shipping",
                shippingOption.getString(PaymentShippingOption.EXTRA_SHIPPING_OPTION_LABEL));
        Bundle amount =
                shippingOption.getBundle(PaymentShippingOption.EXTRA_SHIPPING_OPTION_AMOUNT);
        Assert.assertEquals("CAD", amount.getString(PaymentCurrencyAmount.EXTRA_CURRENCY));
        Assert.assertEquals("0.00", amount.getString(PaymentCurrencyAmount.EXTRA_VALUE));
        Assert.assertTrue(
                shippingOption.getBoolean(PaymentShippingOption.EXTRA_SHIPPING_OPTION_SELECTED));

        Assert.assertEquals(
                "error message",
                mUpdatedPaymentDetails.getString(PaymentRequestDetailsUpdate.EXTRA_ERROR_MESSAGE));
        Assert.assertEquals(
                "stringified payment method",
                mUpdatedPaymentDetails.getString(
                        PaymentRequestDetailsUpdate.EXTRA_STRINGIFIED_PAYMENT_METHOD_ERRORS));

        // Validate address errors
        Bundle addressError =
                mUpdatedPaymentDetails.getBundle(PaymentRequestDetailsUpdate.EXTRA_ADDRESS_ERRORS);
        Assert.assertEquals("invalid address line", addressError.getString("addressLine"));
        Assert.assertEquals("invalid city", addressError.getString("city"));
        Assert.assertEquals("invalid country code", addressError.getString("countryCode"));
        Assert.assertEquals(
                "invalid dependent locality", addressError.getString("dependentLocality"));
        Assert.assertEquals("invalid organization", addressError.getString("organization"));
        Assert.assertEquals("invalid phone", addressError.getString("phone"));
        Assert.assertEquals("invalid postal code", addressError.getString("postalCode"));
        Assert.assertEquals("invalid recipient", addressError.getString("recipient"));
        Assert.assertEquals("invalid region", addressError.getString("region"));
        Assert.assertEquals("invalid sorting code", addressError.getString("sortingCode"));
    }

    private void startPaymentDetailsUpdateService() {
        Intent intent =
                new Intent(
                        /*ContextUtils.getApplicationContext()*/ mContext,
                        PaymentDetailsUpdateService.class);
        intent.setAction(IPaymentDetailsUpdateService.class.getName());
        mContext.bindService(intent, mConnection, Context.BIND_AUTO_CREATE);

        CriteriaHelper.pollUiThread(
                () -> {
                    return mBound;
                },
                DECODER_STARTUP_TIMEOUT_IN_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private boolean mMethodChangeListenerNotified;
    private boolean mShippingOptionChangeListenerNotified;
    private boolean mShippingAddressChangeListenerNotified;
    private PaymentRequestUpdateEventListener mUpdateListener =
            new FakePaymentRequestUpdateEventListener();

    private class FakePaymentRequestUpdateEventListener
            implements PaymentRequestUpdateEventListener {
        @Override
        public boolean changePaymentMethodFromInvokedApp(
                String methodName, String stringifiedDetails) {
            Assert.assertFalse(TextUtils.isEmpty(methodName));
            mMethodChangeListenerNotified = true;
            return true;
        }

        @Override
        public boolean changeShippingOptionFromInvokedApp(String shippingOptionId) {
            Assert.assertFalse(TextUtils.isEmpty(shippingOptionId));
            mShippingOptionChangeListenerNotified = true;
            return true;
        }

        @Override
        public boolean changeShippingAddressFromInvokedApp(PaymentAddress shippingAddress) {
            mShippingAddressChangeListenerNotified = true;
            return true;
        }
    }

    private Bundle mUpdatedPaymentDetails;
    private boolean mPaymentDetailsDidNotUpdate;

    private class PaymentDetailsUpdateServiceCallback
            extends IPaymentDetailsUpdateServiceCallback.Stub {
        @Override
        public void updateWith(Bundle updatedPaymentDetails) {
            mUpdatedPaymentDetails = updatedPaymentDetails;
        }

        @Override
        public void paymentDetailsNotUpdated() {
            mPaymentDetailsDidNotUpdate = true;
        }
    }

    private String receivedErrorString() {
        return mUpdatedPaymentDetails.getString(
                PaymentRequestDetailsUpdate.EXTRA_ERROR_MESSAGE, "");
    }

    private void verifyIsWaitingForPaymentDetailsUpdate(boolean expected) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            expected,
                            PaymentDetailsUpdateServiceHelper.getInstance()
                                    .isWaitingForPaymentDetailsUpdate());
                });
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testConnectWhenPaymentAppNotInvoked() throws Throwable {
        installPaymentApp();
        startPaymentDetailsUpdateService();
        mIPaymentDetailsUpdateService.changePaymentMethod(
                defaultMethodDataBundle(), new PaymentDetailsUpdateServiceCallback());
        verifyIsWaitingForPaymentDetailsUpdate(false);
        Assert.assertFalse(mMethodChangeListenerNotified);
        // An unauthorized app won't get a callback with error.
        Assert.assertEquals(null, mUpdatedPaymentDetails);
        Assert.assertFalse(mPaymentDetailsDidNotUpdate);
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testBindHistogramRecordedWhenConnected() throws Throwable {
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "PaymentRequest.PaymentDetailsUpdateService.Bind", true)) {
            // No payment flow needs to be happening for recording the metric of a service
            // connection to Chrome.
            startPaymentDetailsUpdateService();
        }
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSuccessfulChangePaymentMethod() throws Throwable {
        installAndInvokePaymentApp();
        startPaymentDetailsUpdateService();
        mIPaymentDetailsUpdateService.changePaymentMethod(
                defaultMethodDataBundle(), new PaymentDetailsUpdateServiceCallback());
        verifyIsWaitingForPaymentDetailsUpdate(true);
        Assert.assertTrue(mMethodChangeListenerNotified);
        updateWithDefaultDetails();
        verifyUpdatedDefaultDetails();
        verifyIsWaitingForPaymentDetailsUpdate(false);
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testChangePaymentMethodHistogram() throws Throwable {
        installAndInvokePaymentApp();
        startPaymentDetailsUpdateService();
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "PaymentRequest.PaymentDetailsUpdateService.ChangePaymentMethod", true);

        mIPaymentDetailsUpdateService.changePaymentMethod(
                defaultMethodDataBundle(), new PaymentDetailsUpdateServiceCallback());

        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testChangePaymentMethodMissingBundle() throws Throwable {
        installAndInvokePaymentApp();
        startPaymentDetailsUpdateService();
        mIPaymentDetailsUpdateService.changePaymentMethod(
                /* paymentHandlerMethodData= */ null, new PaymentDetailsUpdateServiceCallback());
        verifyIsWaitingForPaymentDetailsUpdate(false);
        Assert.assertFalse(mMethodChangeListenerNotified);
        Assert.assertEquals(ErrorStrings.METHOD_DATA_REQUIRED, receivedErrorString());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testChangePaymentMethodMissingMethodNameBundle() throws Throwable {
        installAndInvokePaymentApp();
        startPaymentDetailsUpdateService();
        Bundle bundle = new Bundle();
        bundle.putString(
                PaymentHandlerMethodData.EXTRA_STRINGIFIED_DETAILS, "{\"key\": \"value\"}");
        mIPaymentDetailsUpdateService.changePaymentMethod(
                bundle, new PaymentDetailsUpdateServiceCallback());
        verifyIsWaitingForPaymentDetailsUpdate(false);
        Assert.assertFalse(mMethodChangeListenerNotified);
        Assert.assertEquals(ErrorStrings.METHOD_NAME_REQUIRED, receivedErrorString());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSuccessfulChangePaymentMethodWithMissingDetails() throws Throwable {
        installAndInvokePaymentApp();
        startPaymentDetailsUpdateService();
        Bundle bundle = new Bundle();
        bundle.putString(PaymentHandlerMethodData.EXTRA_METHOD_NAME, "method-name");
        // Skip populating "PaymentHandlerMethodData.EXTRA_STRINGIFIED_DETAILS" to verify that it is
        // not a mandatory field.
        mIPaymentDetailsUpdateService.changePaymentMethod(
                bundle, new PaymentDetailsUpdateServiceCallback());
        verifyIsWaitingForPaymentDetailsUpdate(true);
        Assert.assertTrue(mMethodChangeListenerNotified);
        updateWithDefaultDetails();
        verifyUpdatedDefaultDetails();
        verifyIsWaitingForPaymentDetailsUpdate(false);
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSuccessfulChangeShippingOption() throws Throwable {
        installAndInvokePaymentApp();
        startPaymentDetailsUpdateService();
        mIPaymentDetailsUpdateService.changeShippingOption(
                "shipping option id", new PaymentDetailsUpdateServiceCallback());
        verifyIsWaitingForPaymentDetailsUpdate(true);
        Assert.assertTrue(mShippingOptionChangeListenerNotified);
        updateWithDefaultDetails();
        verifyUpdatedDefaultDetails();
        verifyIsWaitingForPaymentDetailsUpdate(false);
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testChangeShippingOptionWithMissingOptionId() throws Throwable {
        installAndInvokePaymentApp();
        startPaymentDetailsUpdateService();
        mIPaymentDetailsUpdateService.changeShippingOption(
                /* shippingOptionId= */ "", new PaymentDetailsUpdateServiceCallback());
        verifyIsWaitingForPaymentDetailsUpdate(false);
        Assert.assertFalse(mShippingOptionChangeListenerNotified);
        Assert.assertEquals(ErrorStrings.SHIPPING_OPTION_ID_REQUIRED, receivedErrorString());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testChangeShippingOptionHistogram() throws Throwable {
        installAndInvokePaymentApp();
        startPaymentDetailsUpdateService();
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "PaymentRequest.PaymentDetailsUpdateService.ChangeShippingOption", true);

        mIPaymentDetailsUpdateService.changeShippingOption(
                /* shippingOptionId= */ "", new PaymentDetailsUpdateServiceCallback());

        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSuccessfulChangeShippingAddress() throws Throwable {
        installAndInvokePaymentApp();
        startPaymentDetailsUpdateService();
        mIPaymentDetailsUpdateService.changeShippingAddress(
                defaultAddressBundle(), new PaymentDetailsUpdateServiceCallback());
        verifyIsWaitingForPaymentDetailsUpdate(true);
        Assert.assertTrue(mShippingAddressChangeListenerNotified);
        updateWithDefaultDetails();
        verifyUpdatedDefaultDetails();
        verifyIsWaitingForPaymentDetailsUpdate(false);
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testChangeShippingAddressHistogram() throws Throwable {
        installAndInvokePaymentApp();
        startPaymentDetailsUpdateService();
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "PaymentRequest.PaymentDetailsUpdateService.ChangeShippingAddress", true);

        mIPaymentDetailsUpdateService.changeShippingAddress(
                /* shippingAddress= */ null, new PaymentDetailsUpdateServiceCallback());

        histogramWatcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testChangeShippingAddressWithMissingBundle() throws Throwable {
        installAndInvokePaymentApp();
        startPaymentDetailsUpdateService();
        mIPaymentDetailsUpdateService.changeShippingAddress(
                /* shippingAddress= */ null, new PaymentDetailsUpdateServiceCallback());
        verifyIsWaitingForPaymentDetailsUpdate(false);
        Assert.assertFalse(mShippingAddressChangeListenerNotified);
        Assert.assertEquals(ErrorStrings.SHIPPING_ADDRESS_INVALID, receivedErrorString());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testChangeShippingAddressWithInvalidCountryCode() throws Throwable {
        installAndInvokePaymentApp();
        startPaymentDetailsUpdateService();
        Bundle invalidAddress = defaultAddressBundle();
        invalidAddress.putString(Address.EXTRA_ADDRESS_COUNTRY, "");
        mIPaymentDetailsUpdateService.changeShippingAddress(
                invalidAddress, new PaymentDetailsUpdateServiceCallback());
        verifyIsWaitingForPaymentDetailsUpdate(false);
        Assert.assertFalse(mShippingAddressChangeListenerNotified);
        Assert.assertEquals(ErrorStrings.SHIPPING_ADDRESS_INVALID, receivedErrorString());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testChangeWhileWaitingForPaymentDetailsUpdate() throws Throwable {
        installAndInvokePaymentApp();
        startPaymentDetailsUpdateService();
        mIPaymentDetailsUpdateService.changePaymentMethod(
                defaultMethodDataBundle(), new PaymentDetailsUpdateServiceCallback());
        verifyIsWaitingForPaymentDetailsUpdate(true);
        Assert.assertTrue(mMethodChangeListenerNotified);

        // Call changeShippingOption while waiting for updated payment details.
        mIPaymentDetailsUpdateService.changeShippingOption(
                "shipping option id", new PaymentDetailsUpdateServiceCallback());
        verifyIsWaitingForPaymentDetailsUpdate(true);
        Assert.assertFalse(mShippingOptionChangeListenerNotified);
        Assert.assertEquals(ErrorStrings.INVALID_STATE, receivedErrorString());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPaymentDetailsNotUpdated() throws Throwable {
        installAndInvokePaymentApp();
        startPaymentDetailsUpdateService();
        mIPaymentDetailsUpdateService.changeShippingOption(
                "shipping option id", new PaymentDetailsUpdateServiceCallback());
        verifyIsWaitingForPaymentDetailsUpdate(true);
        Assert.assertTrue(mShippingOptionChangeListenerNotified);
        onPaymentDetailsNotUpdated();
        Assert.assertTrue(mPaymentDetailsDidNotUpdate);
        verifyIsWaitingForPaymentDetailsUpdate(false);
    }
}
