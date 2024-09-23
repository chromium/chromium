// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.mockito.AdditionalAnswers.answerVoid;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.IsReadyToPayService;
import org.chromium.IsReadyToPayServiceCallback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.payments.intent.IsReadyToPayServiceHelper;

/** Tests for IsReadyToPayServiceHelper. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IsReadyToPayServiceHelperTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule public final ExpectedException mExpectedExceptionRule = ExpectedException.none();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private IBinder mBinderMock;
    @Spy private IsReadyToPayService.Default mServiceSpy;

    private boolean mErrorReceived;
    private boolean mResponseReceived;

    @Before
    public void setUp() throws Throwable {
        Looper.prepare();
    }

    @After
    public void tearDown() throws Throwable {}

    private interface ServiceCallbackHandler {
        void handle(IsReadyToPayServiceCallback callback);
    }

    // Create a mock service that does not respond to the IsReadyToPay query.
    private IBinder createUnresponsiveService() {
        // Intentionally leave blank to be unresponsive.
        return createService((serviceCallback) -> {});
    }

    // Create a mock service that always responds being ready.
    private IBinder createAlwaysReadyService() {
        return createService(
                (serviceCallback) -> {
                    new Handler()
                            .post(
                                    () -> {
                                        try {
                                            serviceCallback.handleIsReadyToPay(true);
                                        } catch (Throwable e) {
                                            Assert.fail(e.toString());
                                        }
                                    });
                });
    }

    private IBinder createService(ServiceCallbackHandler serviceCallbackHandler) {
        IBinder binder = Mockito.mock(IBinder.class);
        Mockito.when(binder.queryLocalInterface(Mockito.any())).thenReturn(mServiceSpy);

        try {
            Mockito.doAnswer(
                            answerVoid(
                                    (IsReadyToPayServiceCallback callback) ->
                                            serviceCallbackHandler.handle(callback)))
                    .when(mServiceSpy)
                    .isReadyToPay(Mockito.any(IsReadyToPayServiceHelper.class));

        } catch (Throwable e) {
            Assert.fail(e.toString());
        }
        return binder;
    }

    /**
     * Create a mock context that can run a specified service.
     *
     * @param serviceBinder The binder of a service to run. Null means no service.
     */
    private Context createContext(IBinder serviceBinder) {
        Context context = Mockito.mock(Context.class);

        // Mock {@link Context#bindService}.
        try {
            Mockito.doAnswer(
                            (invocation) -> {
                                if (serviceBinder == null) return false;
                                ServiceConnection serviceConnection = invocation.getArgument(1);
                                new Handler()
                                        .post(
                                                () -> {
                                                    ComponentName mockComponentName = null;
                                                    try {
                                                        mockComponentName =
                                                                Mockito.any(ComponentName.class);
                                                    } catch (Throwable e) {
                                                        Assert.fail(e.toString());
                                                    }

                                                    serviceConnection.onServiceConnected(
                                                            mockComponentName, serviceBinder);
                                                });
                                return true;
                            })
                    .when(context)
                    .bindService(
                            Mockito.any(Intent.class),
                            Mockito.any(ServiceConnection.class),
                            Mockito.anyInt());
        } catch (Throwable e) {
            Assert.fail(e.toString());
        }
        return context;
    }

    /** Create a mock context that never connects to a service. */
    private Context createContextThatNeverConnectToService() {
        Context context = Mockito.mock(Context.class);

        // Mock {@link Context#bindService}.
        try {
            Mockito.doAnswer(
                            (invocation) -> {
                                ServiceConnection serviceConnection = invocation.getArgument(1);
                                new Handler()
                                        .post(
                                                () -> {
                                                    ComponentName mockComponentName = null;
                                                    try {
                                                        mockComponentName =
                                                                Mockito.any(ComponentName.class);
                                                    } catch (Throwable e) {
                                                        Assert.fail(e.toString());
                                                    }
                                                });
                                return true;
                            })
                    .when(context)
                    .bindService(
                            Mockito.any(Intent.class),
                            Mockito.any(ServiceConnection.class),
                            Mockito.anyInt());
        } catch (Throwable e) {
            Assert.fail(e.toString());
        }
        return context;
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void onErrorTest() throws Throwable {
        mErrorReceived = false;
        Intent intent = new Intent();
        intent.setClassName("mock.package.name", "mock.service.name");
        Context context = ApplicationProvider.getApplicationContext();
        IsReadyToPayServiceHelper helper =
                new IsReadyToPayServiceHelper(
                        context,
                        intent,
                        new IsReadyToPayServiceHelper.ResultHandler() {
                            @Override
                            public void onIsReadyToPayServiceResponse(boolean isReadyToPay) {
                                Assert.fail("IsReadyToPayService should not respond.");
                            }

                            @Override
                            public void onIsReadyToPayServiceError() {
                                mErrorReceived = true;
                            }
                        });
        helper.query();
        CriteriaHelper.pollInstrumentationThread(() -> mErrorReceived);
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void onResponseTest() throws Throwable {
        mResponseReceived = false;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Intent intent = new Intent();
                    intent.setClassName("mock.package.name", "mock.service.name");
                    Context context = createContext(createAlwaysReadyService());
                    IsReadyToPayServiceHelper helper =
                            new IsReadyToPayServiceHelper(
                                    context,
                                    intent,
                                    new IsReadyToPayServiceHelper.ResultHandler() {
                                        @Override
                                        public void onIsReadyToPayServiceResponse(
                                                boolean isReadyToPay) {
                                            mResponseReceived = true;
                                        }

                                        @Override
                                        public void onIsReadyToPayServiceError() {
                                            Assert.fail();
                                        }
                                    });
                    helper.query();
                });
        CriteriaHelper.pollInstrumentationThread(() -> mResponseReceived);
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void unresponsiveServiceTest() throws Throwable {
        mErrorReceived = false;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Intent intent = new Intent();
                    intent.setClassName("mock.package.name", "mock.service.name");
                    Context context = createContext(createUnresponsiveService());
                    IsReadyToPayServiceHelper helper =
                            new IsReadyToPayServiceHelper(
                                    context,
                                    intent,
                                    new IsReadyToPayServiceHelper.ResultHandler() {
                                        @Override
                                        public void onIsReadyToPayServiceResponse(
                                                boolean isReadyToPay) {
                                            Assert.fail();
                                        }

                                        @Override
                                        public void onIsReadyToPayServiceError() {
                                            mErrorReceived = true;
                                        }
                                    });
                    helper.query();
                });
        CriteriaHelper.pollInstrumentationThread(() -> mErrorReceived);
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void noServiceTest() throws Throwable {
        mErrorReceived = false;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Intent intent = new Intent();
                    intent.setClassName("mock.package.name", "mock.service.name");
                    Context context = createContext(/* serviceBinder= */ null);
                    IsReadyToPayServiceHelper helper =
                            new IsReadyToPayServiceHelper(
                                    context,
                                    intent,
                                    new IsReadyToPayServiceHelper.ResultHandler() {
                                        @Override
                                        public void onIsReadyToPayServiceResponse(
                                                boolean isReadyToPay) {
                                            Assert.fail();
                                        }

                                        @Override
                                        public void onIsReadyToPayServiceError() {
                                            mErrorReceived = true;
                                        }
                                    });
                    helper.query();
                });
        CriteriaHelper.pollInstrumentationThread(() -> mErrorReceived);
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void serviceConnectionTimeoutTest() throws Throwable {
        mErrorReceived = false;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Intent intent = new Intent();
                    intent.setClassName("mock.package.name", "mock.service.name");
                    Context context = createContextThatNeverConnectToService();
                    IsReadyToPayServiceHelper helper =
                            new IsReadyToPayServiceHelper(
                                    context,
                                    intent,
                                    new IsReadyToPayServiceHelper.ResultHandler() {
                                        @Override
                                        public void onIsReadyToPayServiceResponse(
                                                boolean isReadyToPay) {
                                            Assert.fail();
                                        }

                                        @Override
                                        public void onIsReadyToPayServiceError() {
                                            mErrorReceived = true;
                                        }
                                    });
                    helper.query();
                });
        // Assuming CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL >
        // IsReadyToPayServiceHelper.SERVICE_CONNECTION_TIMEOUT_MS.
        CriteriaHelper.pollInstrumentationThread(() -> mErrorReceived);
    }
}
