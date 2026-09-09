// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.printing;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.os.Build.VERSION_CODES;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.print.PageRange;
import android.print.PrintAttributes;
import android.print.PrintDocumentAdapter;
import android.print.PrintDocumentInfo;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.TestFileUtil;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.PdfCtaPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DomAutomationController;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.printing.PrintDocumentAdapterWrapper.LayoutResultCallbackWrapper;
import org.chromium.printing.PrintDocumentAdapterWrapper.WriteResultCallbackWrapper;
import org.chromium.printing.PrintManagerDelegate;
import org.chromium.printing.Printable;
import org.chromium.printing.PrintingController;
import org.chromium.printing.PrintingControllerImpl;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.widget.Toast;
import org.chromium.ui.widget.ToastManager;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests Android printing. TODO(cimamoglu): Add a test with cancellation. TODO(cimamoglu): Add a
 * test with multiple, stacked onLayout/onWrite calls. TODO(cimamoglu): Add a test which emulates
 * Chromium failing to generate a PDF.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PrintingControllerTest {
    @Rule
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private static final String TEMP_FILE_NAME = "temp_print";
    private static final String TEMP_FILE_EXTENSION = ".pdf";
    private static final String URL =
            UrlUtils.encodeHtmlDataUri("<html><head></head><body>foo</body></html>");
    private static final String PDF_PREAMBLE = "%PDF-1";
    private static final long TEST_TIMEOUT = 20000L;
    private static final long PDF_LOAD_TIMEOUT_MS = 8000;
    private static final long POLLING_INTERVAL_MS = 500;

    @Before
    public void setUp() {
        // Do nothing.
    }

    private static class LayoutResultCallbackWrapperMock implements LayoutResultCallbackWrapper {
        @Override
        public void onLayoutFinished(PrintDocumentInfo info, boolean changed) {}

        @Override
        public void onLayoutFailed(CharSequence error) {}

        @Override
        public void onLayoutCancelled() {}
    }

    private static class WriteResultCallbackWrapperMock implements WriteResultCallbackWrapper {
        @Override
        public void onWriteFinished(PageRange[] pages) {}

        @Override
        public void onWriteFailed(CharSequence error) {}

        @Override
        public void onWriteCancelled() {}
    }

    private static class WaitForOnWriteHelper extends CallbackHelper {
        public void waitForCallback(String msg) throws TimeoutException {
            waitForOnly(msg, TEST_TIMEOUT, TimeUnit.MILLISECONDS);
        }
    }

    private static class TemporaryFileHandler implements AutoCloseable {
        private final File mTempFile;
        private final ParcelFileDescriptor mFileDescriptor;

        public TemporaryFileHandler() throws IOException {
            mTempFile = File.createTempFile(TEMP_FILE_NAME, TEMP_FILE_EXTENSION);
            try {
                mFileDescriptor =
                        ParcelFileDescriptor.open(mTempFile, ParcelFileDescriptor.MODE_READ_WRITE);
            } catch (FileNotFoundException e) {
                // Exception happened, can't continue, cleanup the file.
                TestFileUtil.deleteFile(mTempFile.getAbsolutePath());
                throw new FileNotFoundException();
            }
        }

        ParcelFileDescriptor getFileDescriptor() {
            return mFileDescriptor;
        }

        @Override
        public void close() throws IOException {
            try {
                mFileDescriptor.close();
            } finally {
                TestFileUtil.deleteFile(mTempFile.getAbsolutePath());
            }
        }
    }

    private static class PrintingControllerImplPdfWritingDone extends PrintingControllerImpl {
        private final WaitForOnWriteHelper mWaitForOnWrite;

        public PrintingControllerImplPdfWritingDone(
                WindowAndroid window, WaitForOnWriteHelper waitForOnWrite) {
            super(window);
            mWaitForOnWrite = waitForOnWrite;
            setPrintingControllerForTesting(window, this);
        }

        @Override
        public void pdfWritingDone(int pageCount) {
            mWaitForOnWrite.notifyCalled();
        }
    }

    /** Test a basic printing flow on web page. */
    @Test
    @LargeTest
    @Feature({"Printing"})
    public void testNormalPrintingFlow() throws Throwable {
        WebPageStation page = mActivityTestRule.startOnUrl(URL);
        testNormalPrintingFlowHelper(page.getTab());
    }

    /** Test a basic printing flow on pdf page. */
    @Test
    @LargeTest
    @Feature({"Printing"})
    @MinAndroidSdkLevel(VERSION_CODES.VANILLA_ICE_CREAM)
    public void testNormalPrintingFlow_PDF() throws Throwable {
        WebPageStation page =
                mActivityTestRule.startOnTestServerUrl("/pdf/test/data/hello_world2.pdf");
        Tab currentTab = page.getTab();
        // Wait for PDF page to load.
        CriteriaHelper.pollUiThread(
                () -> {
                    if (!currentTab.isNativePage()) {
                        return false;
                    }
                    return currentTab.getNativePage().getCanonicalFilepath() != null;
                },
                "PDF page is not loaded successfully.",
                PDF_LOAD_TIMEOUT_MS,
                POLLING_INTERVAL_MS);
        testNormalPrintingFlowHelper(currentTab);
    }

    /** Test a basic printing flow on pdf page in incognito mode. */
    @Test
    @LargeTest
    @Feature({"Printing"})
    @EnableFeatures({ChromeFeatureList.INLINE_PDF_V2, ChromeFeatureList.INLINE_PDF_V2_INCOGNITO})
    @MinAndroidSdkLevel(VERSION_CODES.VANILLA_ICE_CREAM)
    public void testNormalPrintingFlow_PDF_Incognito() throws Throwable {
        WebPageStation incognitoPage = mActivityTestRule.startOnIncognitoBlankPage();
        EmbeddedTestServer testServer = mActivityTestRule.getTestServer();
        final String url = testServer.getURL("/pdf/test/data/hello_world2.pdf");
        PdfCtaPageStation pdfPage =
                incognitoPage.openFakeLink(url, PdfCtaPageStation.newBuilder());
        Tab currentTab = pdfPage.getTab();
        testNormalPrintingFlowHelper(currentTab);
    }

    /**
     * Test for http://crbug.com/40434612 Simulating while a printing job is triggered and about to
     * call Android framework to show UI, the corresponding tab is closed, this behaviour is mostly
     * from JavaScript code. Make sure we don't crash and won't call into framework.
     */
    @Test
    @MediumTest
    @Feature({"Printing"})
    public void testPrintCloseWindowBeforeStart() {
        WebPageStation page = mActivityTestRule.startOnUrl(URL);
        Tab currentTab = page.getTab();
        final PrintingControllerImpl printingController = createControllerOnUiThread();
        final PrintManagerDelegate mockPrintManagerDelegate =
                mockPrintManagerDelegate(() -> Assert.fail("Shouldn't start a printing job."));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    printingController.setPendingPrint(
                            new TabPrinter(currentTab), mockPrintManagerDelegate, -1, -1);
                    TabModel currentModel = mActivityTestRule.getActivity().getCurrentTabModel();
                    Tab tab = TabModelUtils.getCurrentTab(currentModel);
                    Assert.assertNotNull(tab);
                    currentModel
                            .getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeTab(tab).allowUndo(false).build(),
                                    /* allowDialog= */ false);
                    Assert.assertFalse(
                            "currentTab should be closed already.", currentTab.isInitialized());
                    printingController.startPendingPrint();
                });
    }

    /**
     * Test for http://crbug.com/40434612 Simulating while a printing job is triggered and printing
     * UI is showing, the corresponding tab is closed, this behaviour is mostly from JavaScript
     * code. Make sure we don't crash and let framework notify user that we can't perform printing
     * job.
     */
    @Test
    @LargeTest
    @Feature({"Printing"})
    public void testPrintCloseWindowBeforeOnWrite() throws Throwable {
        WebPageStation page = mActivityTestRule.startOnUrl(URL);
        Tab currentTab = page.getTab();
        final PrintingControllerImpl printingController = createControllerOnUiThread();

        startControllerOnUiThread(printingController, currentTab);
        callStartOnUiThread(printingController);

        final WaitForOnWriteHelper onWriteFinishedCompleted = new WaitForOnWriteHelper();
        final LayoutResultCallbackWrapper layoutResultCallback =
                new LayoutResultCallbackWrapperMock() {
                    @Override
                    public void onLayoutFinished(PrintDocumentInfo info, boolean changed) {
                        onWriteFinishedCompleted.notifyCalled();
                    }
                };
        callLayoutOnUiThread(
                printingController, null, createPlaceholderPrintAttributes(), layoutResultCallback);

        onWriteFinishedCompleted.waitForCallback("onWriteFinished callback never completed.");

        final WaitForOnWriteHelper onWriteFailedCompleted = new WaitForOnWriteHelper();
        // Create a temporary file to save the PDF.
        final File tempFile = File.createTempFile(TEMP_FILE_NAME, TEMP_FILE_EXTENSION);
        final ParcelFileDescriptor fileDescriptor =
                ParcelFileDescriptor.open(tempFile, ParcelFileDescriptor.MODE_READ_WRITE);
        try {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        // Close tab.
                        TabModel currentModel =
                                mActivityTestRule.getActivity().getCurrentTabModel();
                        Tab tab = TabModelUtils.getCurrentTab(currentModel);
                        Assert.assertNotNull(tab);
                        currentModel
                                .getTabRemover()
                                .closeTabs(
                                        TabClosureParams.closeTab(tab).allowUndo(false).build(),
                                        /* allowDialog= */ false);
                        Assert.assertFalse(
                                "currentTab should be closed already.", currentTab.isInitialized());

                        final WriteResultCallbackWrapper writeResultCallback =
                                new WriteResultCallbackWrapperMock() {
                                    @Override
                                    public void onWriteFailed(CharSequence error) {
                                        onWriteFailedCompleted.notifyCalled();
                                    }
                                };
                        // Call onWrite.
                        printingController.onWrite(
                                new PageRange[] {PageRange.ALL_PAGES},
                                fileDescriptor,
                                new CancellationSignal(),
                                writeResultCallback);
                    });

            onWriteFailedCompleted.waitForCallback("onWriteFailed callback never completed.");
        } finally {
            // Proper cleanup.
            callFinishOnUiThread(printingController);
            // Close the descriptor, if not closed already.
            fileDescriptor.close();
            TestFileUtil.deleteFile(tempFile.getAbsolutePath());
        }
    }

    /**
     * Test for http://crbug.com/41401371 This bug shows Android printing framework could call
     * |PrintDocumentAdapter.onFinish()| before one of |WriteResultCallback.onWrite{Cancelled,
     * Failed, Finished}()| get called. Crash test, pass if there is no crash.
     */
    @Test
    @MediumTest
    @Feature({"Printing"})
    public void testCancelPrintBeforeWriteResultCallbacks() throws Throwable {
        WebPageStation page = mActivityTestRule.startOnUrl(URL);
        Tab currentTab = page.getTab();

        final WaitForOnWriteHelper onWriteHelper = new WaitForOnWriteHelper();
        final PrintingControllerImpl printingController =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                new PrintingControllerImplPdfWritingDone(
                                        mActivityTestRule.getActivity().getWindowAndroid(),
                                        onWriteHelper));

        startControllerOnUiThread(printingController, currentTab);
        callStartOnUiThread(printingController);

        final WriteResultCallbackWrapper writeResultCallback =
                new WriteResultCallbackWrapperMock() {
                    @Override
                    public void onWriteFinished(PageRange[] pages) {
                        Assert.fail("onWriteFinished shouldn't be called");
                    }

                    @Override
                    public void onWriteFailed(CharSequence error) {
                        Assert.fail("onWriteFailed shouldn't be called");
                    }

                    @Override
                    public void onWriteCancelled() {
                        Assert.fail("onWriteCancelled shouldn't be called");
                    }
                };

        try (TemporaryFileHandler handler = new TemporaryFileHandler()) {
            final LayoutResultCallbackWrapper layoutResultCallback =
                    new LayoutResultCallbackWrapperMock() {
                        @Override
                        public void onLayoutFinished(PrintDocumentInfo info, boolean changed) {
                            printingController.onWrite(
                                    new PageRange[] {PageRange.ALL_PAGES},
                                    handler.getFileDescriptor(),
                                    new CancellationSignal(),
                                    writeResultCallback);
                        }
                    };
            callLayoutOnUiThread(
                    printingController,
                    null,
                    createPlaceholderPrintAttributes(),
                    layoutResultCallback);
            onWriteHelper.waitForCallback("pdfWritingDone never called");
            callFinishOnUiThread(printingController);
        }
    }

    /**
     * Regresstion test for crbug.com/40632299. In some cases, native printing code will fail
     * without starting a printing task in Java side. pdfWritingDone() will be called with
     * |pageCount| = 0 in this case. We don't need to do anything for this in Java side for now.
     */
    @Test
    @SmallTest
    @Feature({"Printing"})
    public void testPdfWritingDoneCalledWithoutInitailizePrintingTask() {
        mActivityTestRule.startOnUrl(URL);
        final PrintingControllerImpl controller = createControllerOnUiThread();

        // Calling pdfWritingDone() with |pageCount| = 0 before onWrite() was called. It shouldn't
        // crash.
        ThreadUtils.runOnUiThreadBlocking(() -> controller.pdfWritingDone(0));
    }

    @Test
    @SmallTest
    @Feature({"Printing"})
    public void testGetFileDescriptorAfterFinish() throws Exception {
        WebPageStation page = mActivityTestRule.startOnUrl(URL);
        final PrintingControllerImpl controller = createControllerOnUiThread();

        startControllerOnUiThread(controller, page.getTab());

        try (TemporaryFileHandler handler = new TemporaryFileHandler()) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        Assert.assertNull(controller.getParcelFileDescriptor());
                        controller.onStart();

                        // Simulate onWrite to set the file descriptor
                        controller.onWrite(
                                new PageRange[] {PageRange.ALL_PAGES},
                                handler.getFileDescriptor(),
                                new CancellationSignal(),
                                new WriteResultCallbackWrapperMock());

                        // Check that it is now a valid FD (non-null ParcelFileDescriptor)
                        Assert.assertNotNull(controller.getParcelFileDescriptor());

                        controller.onFinish();
                        // Verify it goes back to invalid after finish
                        Assert.assertNull(controller.getParcelFileDescriptor());
                    });
        }
    }

    @Test
    @SmallTest
    @Feature({"Printing"})
    public void testTabPrinterCanPrintHiddenTab() {
        WebPageStation page = mActivityTestRule.startOnUrl(URL);
        ChromeTabbedActivity cta = page.getActivity();

        // ensure two tabs are open.
        TabUiTestHelper.createTabs(cta, false, 2);

        Tab hiddenTab =
                ThreadUtils.runOnUiThreadBlocking(() -> cta.getCurrentTabModel().getTabAt(0));
        Tab currentTab =
                ThreadUtils.runOnUiThreadBlocking(() -> cta.getCurrentTabModel().getTabAt(1));

        // These asserts needs to be on the UI thread since they are inspecting tab
        // state. (well... mainly canPrint() at the moment)
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // hidden (background) tab should not be allowed to print.
                    assertTrue("hiddenTab should be hidden.", hiddenTab.isHidden());
                    assertFalse(
                            "hiddenTab should not be allowed to print.",
                            new TabPrinter(hiddenTab).canPrint());

                    // current tab should be allowed to print.
                    assertFalse("currentTab should not be hidden.", currentTab.isHidden());
                    assertTrue(
                            "currentTab should be allowed to print.",
                            new TabPrinter(currentTab).canPrint());

                    // Hiding the current tab should still allow it to be printed.
                    currentTab.hide(TabHidingType.ACTIVITY_HIDDEN);
                    assertTrue("currentTab should be hidden.", currentTab.isHidden());
                    assertTrue(
                            "currentTab should be allowed to print even when hidden.",
                            new TabPrinter(currentTab).canPrint());
                });
    }

    @Test
    @SmallTest
    @Feature({"Printing"})
    public void testDisallowPrintOnNativePage() {
        mActivityTestRule.startOnBlankPage();
        mActivityTestRule.loadUrl(UrlConstants.HISTORY_URL);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Tab currentTab = ThreadUtils.runOnUiThreadBlocking(() -> cta.getActivityTab());
        ToastManager toastManager = Mockito.mock(ToastManager.class);
        ToastManager.setInstanceForTesting(toastManager);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue("Should be a native page.", currentTab.isNativePage());
                    assertFalse(
                            "Should return false to indicate the print is not allowed (by showing a"
                                    + " toast).",
                            cta.onMenuOrKeyboardAction(R.id.print_id, false));
                });

        ArgumentCaptor<Toast> toastCaptor = ArgumentCaptor.forClass(Toast.class);
        Mockito.verify(toastManager, Mockito.times(1)).requestShow(toastCaptor.capture());
        Assert.assertEquals("This page can't be printed", toastCaptor.getValue().getText());
    }

    @Test
    @MediumTest
    @Feature({"Printing"})
    public void testMultiWindowPrinting() {
        WebPageStation page = mActivityTestRule.startOnUrl(URL);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WindowAndroid window1 = page.getTab().getWindowAndroid();

                    PrintingController controller1 = PrintingControllerImpl.getInstance(window1);
                    Assert.assertNotNull(controller1);
                    Assert.assertSame(controller1, PrintingControllerImpl.getInstance(window1));

                    // fake a new window
                    WindowAndroid window2 =
                            new WindowAndroid(mActivityTestRule.getActivity(), false);
                    PrintingController controller2 = PrintingControllerImpl.getInstance(window2);

                    // ensure we can have more than one print controller
                    Assert.assertNotNull(controller2);
                    Assert.assertNotSame(controller1, controller2);
                    window2.destroy();
                });
    }

    /**
     * Test a basic printing flow by emulating the corresponding system calls to the printing
     * controller: onStart, onLayout, onWrite, onFinish. Each one is called once, and in this order,
     * in the UI thread.
     */
    private void testNormalPrintingFlowHelper(Tab currentTab) throws Throwable {
        final PrintingControllerImpl printingController = createControllerOnUiThread();

        startControllerOnUiThread(printingController, currentTab);
        // {@link PrintDocumentAdapter#onStart} is always called first.
        callStartOnUiThread(printingController);

        // Create a temporary file to save the PDF.
        final File tempFile = File.createTempFile(TEMP_FILE_NAME, TEMP_FILE_EXTENSION);
        final ParcelFileDescriptor fileDescriptor =
                ParcelFileDescriptor.open(tempFile, ParcelFileDescriptor.MODE_READ_WRITE);

        // Use this to wait for PDF generation to complete, as it will happen asynchronously.
        final WaitForOnWriteHelper onWriteFinishedCompleted = new WaitForOnWriteHelper();

        final WriteResultCallbackWrapper writeResultCallback =
                new WriteResultCallbackWrapperMock() {
                    @Override
                    public void onWriteFinished(PageRange[] pages) {
                        onWriteFinishedCompleted.notifyCalled();
                    }
                };

        final LayoutResultCallbackWrapper layoutResultCallback =
                new LayoutResultCallbackWrapperMock() {
                    // Called on UI thread.
                    @Override
                    public void onLayoutFinished(PrintDocumentInfo info, boolean changed) {
                        printingController.onWrite(
                                new PageRange[] {PageRange.ALL_PAGES},
                                fileDescriptor,
                                new CancellationSignal(),
                                writeResultCallback);
                    }
                };

        callLayoutOnUiThread(
                printingController, null, createPlaceholderPrintAttributes(), layoutResultCallback);

        FileInputStream in = null;
        try {
            onWriteFinishedCompleted.waitForCallback("onWriteFinished callback never completed.");
            Assert.assertTrue(tempFile.length() > 0);
            in = new FileInputStream(tempFile);
            byte[] b = new byte[PDF_PREAMBLE.length()];
            in.read(b);
            String preamble = new String(b);
            Assert.assertEquals(PDF_PREAMBLE, preamble);
        } finally {
            if (in != null) in.close();
            callFinishOnUiThread(printingController);
            // Close the descriptor, if not closed already.
            fileDescriptor.close();
            TestFileUtil.deleteFile(tempFile.getAbsolutePath());
        }
    }

    private PrintingControllerImpl createControllerOnUiThread() {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        (PrintingControllerImpl)
                                PrintingControllerImpl.getInstance(
                                        mActivityTestRule.getActivity().getWindowAndroid()));
    }

    private PrintAttributes createPlaceholderPrintAttributes() {
        return new PrintAttributes.Builder()
                .setMediaSize(PrintAttributes.MediaSize.ISO_A4)
                .setResolution(new PrintAttributes.Resolution("foo", "bar", 300, 300))
                .setMinMargins(PrintAttributes.Margins.NO_MARGINS)
                .build();
    }

    private PrintManagerDelegate mockPrintManagerDelegate(final Runnable r) {
        return new PrintManagerDelegate() {
            @Override
            public boolean print(
                    String printJobName,
                    PrintDocumentAdapter documentAdapter,
                    @Nullable PrintAttributes attributes) {
                if (r != null) r.run();
                return true;
            }
        };
    }

    private void startControllerOnUiThread(final PrintingControllerImpl controller, final Tab tab) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    controller.startPrint(
                            new TabPrinter(tab),
                            /* non-op PrintManagerDelegate */ mockPrintManagerDelegate(null));
                });
    }

    private void callStartOnUiThread(final PrintingControllerImpl controller) {
        ThreadUtils.runOnUiThreadBlocking(() -> controller.onStart());
    }

    private void callLayoutOnUiThread(
            final PrintingControllerImpl controller,
            final PrintAttributes oldAttributes,
            final PrintAttributes newAttributes,
            final LayoutResultCallbackWrapper layoutResultCallback) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    controller.onLayout(
                            oldAttributes,
                            newAttributes,
                            new CancellationSignal(),
                            layoutResultCallback,
                            null);
                });
    }

    private void callFinishOnUiThread(final PrintingControllerImpl controller) {
        ThreadUtils.runOnUiThreadBlocking(() -> controller.onFinish());
    }

    @Test
    @MediumTest
    @Feature({"Printing"})
    public void testCleanupOnWindowContextCleanup() {
        WebPageStation page = mActivityTestRule.startOnUrl(URL);

        // Used in our callback to verify that cleanup occurred
        final AtomicBoolean called = new AtomicBoolean(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Set up a callback to verify that cleanup occurred
                    PrintingControllerImpl.setOnDetachCallbackForTesting(() -> called.set(true));
                    WindowAndroid window = page.getTab().getWindowAndroid();
                    PrintingController controller1 = PrintingControllerImpl.getInstance(window);

                    Assert.assertNotNull(controller1);
                    Assert.assertFalse(controller1.hasPrintingFinished());

                    // Simulate host destruction
                    window.getUnownedUserDataHost().destroy();
                });

        // Expect cleanup to be called
        CriteriaHelper.pollInstrumentationThread(
                () -> called.get(), "onDetachedFromHost not called");
    }

    /** Test that the pending print callback is run exactly when onFinish() is invoked. */
    @Test
    @SmallTest
    @Feature({"Printing"})
    public void testPendingPrintCallbackRunOnFinish() throws Throwable {
        WebPageStation page = mActivityTestRule.startOnUrl(URL);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WindowAndroid window = page.getTab().getWindowAndroid();
                    PrintingControllerImpl printingController =
                            (PrintingControllerImpl) PrintingControllerImpl.getInstance(window);

                    Runnable mockCallback = Mockito.mock(Runnable.class);
                    printingController.setPendingPrintCallback(mockCallback);

                    // Verify it hasn't been called yet.
                    Mockito.verify(mockCallback, Mockito.never()).run();

                    // Call onFinish which simulates the Print Spooler finishing.
                    printingController.onFinish();

                    // Verify that the delayed callback is now executed.
                    Mockito.verify(mockCallback, Mockito.times(1)).run();
                });
    }

    /** Test that the pending print callback is run exactly when onDetachedFromHost() is invoked. */
    @Test
    @SmallTest
    @Feature({"Printing"})
    public void testPendingPrintCallbackRunOnDetach() throws Throwable {
        WebPageStation page = mActivityTestRule.startOnUrl(URL);
        WindowAndroid window = page.getTab().getWindowAndroid();

        // Create mock on test thread.
        Runnable mockCallback = Mockito.mock(Runnable.class);

        CountDownLatch detachLatch = new CountDownLatch(1);
        PrintingControllerImpl.setOnDetachCallbackForTesting(detachLatch::countDown);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrintingControllerImpl printingController =
                            (PrintingControllerImpl) PrintingControllerImpl.getInstance(window);
                    printingController.setPendingPrintCallback(mockCallback);

                    // Verify it hasn't been called yet.
                    Mockito.verify(mockCallback, Mockito.never()).run();

                    // Simulate host destruction to trigger detachment.
                    window.getUnownedUserDataHost().destroy();
                });

        // Wait for the detach callback to complete.
        detachLatch.await();

        // Verify that the delayed callback is now executed.
        Mockito.verify(mockCallback, Mockito.times(1)).run();
    }

    private static class TestPrintingControllerImpl extends PrintingControllerImpl {
        private final PrintManagerDelegate mMockPrintManager;
        private final CountDownLatch mPrintStartedLatch = new CountDownLatch(1);

        public TestPrintingControllerImpl(
                WindowAndroid window, PrintManagerDelegate mockPrintManager) {
            super(window);
            mMockPrintManager = mockPrintManager;
        }

        @Override
        public void setPendingPrintCallback(Runnable callback) {
            super.setPendingPrintCallback(callback);
            mPrintStartedLatch.countDown();
        }

        @Override
        public void setPendingPrint(
                Printable printable,
                PrintManagerDelegate printManager,
                int renderProcessId,
                int renderFrameId) {
            super.setPendingPrint(printable, mMockPrintManager, renderProcessId, renderFrameId);
        }

        public void waitForPrintToStart() throws InterruptedException {
            mPrintStartedLatch.await();
        }
    }

    /**
     * Integration test to verify that window.print() blocks JavaScript execution in the renderer
     * until the print dialog is completed (simulated by onFinish).
     */
    @Test
    @LargeTest
    @Feature({"Printing"})
    public void testWindowPrintBlocksJavaScript() throws Throwable {
        WebPageStation page = mActivityTestRule.startOnUrl(URL);
        Tab tab = page.getTab();
        WindowAndroid window = tab.getWindowAndroid();
        WebContents webContents = tab.getWebContents();

        // Create a mock PrintManagerDelegate to intercept print calls and prevent real dialog.
        PrintManagerDelegate mockPrintManager = mockPrintManagerDelegate(null);

        // Inject our TestPrintingControllerImpl on the UI thread to override the default system
        // print manager.
        final TestPrintingControllerImpl[] testController = new TestPrintingControllerImpl[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    testController[0] = new TestPrintingControllerImpl(window, mockPrintManager);
                    PrintingControllerImpl.setPrintingControllerForTesting(
                            window, testController[0]);
                });

        DomAutomationController controller = new DomAutomationController();
        controller.inject(webContents);

        // Start JavaScript execution that calls window.print() and then signals completion.
        // This should block inside window.print() until we trigger onFinish().
        JavaScriptUtils.executeJavaScript(
                webContents, "window.print(); domAutomationController.send(true);");

        // Wait until the print dialog has been requested (renderer is blocked).
        testController[0].waitForPrintToStart();

        // Trigger onFinish on the UI thread to simulate the print dialog finishing and unblock the
        // renderer.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    testController[0].onFinish();
                });

        // Wait for the JS execution to complete.
        String result = controller.waitForResult("JS failed to complete after window.print()");
        Assert.assertEquals("true", result);
    }

    /**
     * Test to verify that if PrintManagerDelegate fails to initiate printing (e.g. Activity is
     * finishing/destroyed or an exception occurs), the pending print callback is invoked
     * immediately and the controller is not stuck in a busy state.
     */
    @Test
    @SmallTest
    @Feature({"Printing"})
    public void testPrintManagerFailureUnblocksPendingCallback() throws Throwable {
        WebPageStation page = mActivityTestRule.startOnUrl(URL);
        Tab tab = page.getTab();
        WindowAndroid window = tab.getWindowAndroid();

        PrintManagerDelegate failingPrintManager =
                new PrintManagerDelegate() {
                    @Override
                    public boolean print(
                            String printJobName,
                            PrintDocumentAdapter documentAdapter,
                            @Nullable PrintAttributes attributes) {
                        return false;
                    }
                };

        Runnable mockCallback = Mockito.mock(Runnable.class);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrintingControllerImpl printingController =
                            (PrintingControllerImpl) PrintingControllerImpl.getInstance(window);
                    printingController.setPendingPrint(
                            new TabPrinter(tab), failingPrintManager, -1, -1);
                    printingController.setPendingPrintCallback(mockCallback);

                    printingController.startPendingPrint();

                    // The callback should have been invoked immediately because print failed.
                    Mockito.verify(mockCallback, Mockito.times(1)).run();
                    Assert.assertFalse(printingController.isBusy());
                    Assert.assertTrue(printingController.hasPrintingFinished());
                });
    }

    /**
     * Test to verify that calling startPendingPrint() when already busy does not clobber the
     * in-flight job's state or invoke callbacks prematurely.
     */
    @Test
    @SmallTest
    @Feature({"Printing"})
    public void testStartPendingPrintWhenBusyDoesNotClobberState() throws Throwable {
        WebPageStation page = mActivityTestRule.startOnUrl(URL);
        Tab tab = page.getTab();
        WindowAndroid window = tab.getWindowAndroid();

        PrintManagerDelegate successfulPrintManager =
                new PrintManagerDelegate() {
                    @Override
                    public boolean print(
                            String printJobName,
                            PrintDocumentAdapter documentAdapter,
                            @Nullable PrintAttributes attributes) {
                        return true;
                    }
                };

        Runnable pendingCallback = Mockito.mock(Runnable.class);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrintingControllerImpl printingController =
                            (PrintingControllerImpl) PrintingControllerImpl.getInstance(window);
                    printingController.startPrint(new TabPrinter(tab), successfulPrintManager);
                    Assert.assertTrue(printingController.isBusy());

                    // Calling startPendingPrint while busy should be a safe no-op.
                    printingController.setPendingPrintCallback(pendingCallback);
                    printingController.startPendingPrint();

                    // The controller should remain busy with the original job.
                    Assert.assertTrue(printingController.isBusy());
                    Mockito.verify(pendingCallback, Mockito.never()).run();

                    // Cleanup
                    printingController.onActivityDestroyed();
                });
    }

    /**
     * Test to verify that if the Activity is finishing, calling startPendingPrint() will
     * immediately invoke the pending print callback and not start printing.
     */
    @Test
    @SmallTest
    @Feature({"Printing"})
    public void testStartPendingPrintWhenActivityIsFinishing() throws Throwable {
        WebPageStation page = mActivityTestRule.startOnUrl(URL);
        Tab tab = page.getTab();
        WindowAndroid window = tab.getWindowAndroid();
        Activity activity = window.getActivity().get();
        Assert.assertNotNull(activity);

        PrintManagerDelegate failIfCalledPrintManager =
                new PrintManagerDelegate() {
                    @Override
                    public boolean print(
                            String printJobName,
                            PrintDocumentAdapter documentAdapter,
                            @Nullable PrintAttributes attributes) {
                        Assert.fail("print() must not be called for a finishing Activity.");
                        return false;
                    }
                };

        Runnable mockCallback = Mockito.mock(Runnable.class);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrintingControllerImpl printingController =
                            (PrintingControllerImpl) PrintingControllerImpl.getInstance(window);
                    printingController.setPendingPrint(
                            new TabPrinter(tab), failIfCalledPrintManager, -1, -1);
                    printingController.setPendingPrintCallback(mockCallback);

                    activity.finish();
                    Assert.assertTrue(activity.isFinishing());

                    printingController.startPendingPrint();

                    // The callback should have been invoked immediately because the Activity is
                    // finishing.
                    Mockito.verify(mockCallback, Mockito.times(1)).run();
                    Assert.assertFalse(printingController.isBusy());
                    Assert.assertTrue(printingController.hasPrintingFinished());

                    // Cleanup
                    printingController.onActivityDestroyed();
                });
    }

    /**
     * Test the 3-stage asynchronous print lifecycle (InitiatePrint -> RenderPages -> FinishPrint)
     * for a canvas-rendering page that sets up content on beforeprint and cleans up on afterprint.
     */
    @Test
    @MediumTest
    @Feature({"Printing"})
    public void testThreeStageAsyncPrintLifecycleWithCanvas() throws Throwable {
        final String canvasHtml =
                "<html><head><script>"
                        + "window.beforePrintCount = 0;"
                        + "window.afterPrintCount = 0;"
                        + "window.canvasRenderedAsync = false;"
                        + "window.addEventListener('beforeprint', () => {"
                        + "  window.beforePrintCount++;"
                        + "  window.requestAnimationFrame(() => {"
                        + "    const canvas = document.getElementById('c');"
                        + "    if (canvas) {"
                        + "      const ctx = canvas.getContext('2d');"
                        + "      ctx.fillStyle = 'green';"
                        + "      ctx.fillRect(0, 0, 50, 50);"
                        + "    }"
                        + "    window.canvasRenderedAsync = true;"
                        + "  });"
                        + "});"
                        + "window.addEventListener('afterprint', () => {"
                        + "  window.afterPrintCount++;"
                        + "});"
                        + "</script></head>"
                        + "<body><canvas id='c' width='50' height='50'></canvas></body></html>";

        WebPageStation page = mActivityTestRule.startOnUrl(UrlUtils.encodeHtmlDataUri(canvasHtml));
        Tab tab = page.getTab();
        WebContents webContents = tab.getWebContents();
        final PrintingControllerImpl printingController = createControllerOnUiThread();

        startControllerOnUiThread(printingController, tab);

        // Stage 1: onStart (InitiatePrint) - should dispatch beforeprint and allow async rendering.
        callStartOnUiThread(printingController);

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        String count =
                                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                        webContents, "window.beforePrintCount.toString()");
                        return "\"1\"".equals(count);
                    } catch (TimeoutException e) {
                        return false;
                    }
                },
                "beforeprint event was not dispatched during onStart");

        // Verify async canvas rendering completed across the event loop while print session was
        // initiated but before onWrite is triggered.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        String rendered =
                                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                        webContents, "window.canvasRenderedAsync.toString()");
                        return "\"true\"".equals(rendered);
                    } catch (TimeoutException e) {
                        return false;
                    }
                },
                "async canvas rendering did not complete across the event loop");

        // Verify afterprint has NOT been dispatched yet.
        String afterCountBeforeWrite =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        webContents, "window.afterPrintCount.toString()");
        Assert.assertEquals("\"0\"", afterCountBeforeWrite);

        // Stage 2: onLayout and onWrite (RenderPages to PDF)
        final File tempFile = File.createTempFile(TEMP_FILE_NAME, TEMP_FILE_EXTENSION);
        final ParcelFileDescriptor fileDescriptor =
                ParcelFileDescriptor.open(tempFile, ParcelFileDescriptor.MODE_READ_WRITE);
        final WaitForOnWriteHelper onWriteFinishedCompleted = new WaitForOnWriteHelper();

        final WriteResultCallbackWrapper writeResultCallback =
                new WriteResultCallbackWrapperMock() {
                    @Override
                    public void onWriteFinished(PageRange[] pages) {
                        onWriteFinishedCompleted.notifyCalled();
                    }
                };

        final LayoutResultCallbackWrapper layoutResultCallback =
                new LayoutResultCallbackWrapperMock() {
                    @Override
                    public void onLayoutFinished(PrintDocumentInfo info, boolean changed) {
                        printingController.onWrite(
                                new PageRange[] {PageRange.ALL_PAGES},
                                fileDescriptor,
                                new CancellationSignal(),
                                writeResultCallback);
                    }
                };

        callLayoutOnUiThread(
                printingController, null, createPlaceholderPrintAttributes(), layoutResultCallback);

        FileInputStream in = null;
        try {
            onWriteFinishedCompleted.waitForCallback("onWriteFinished callback never completed.");
            Assert.assertTrue(tempFile.length() > 0);
            in = new FileInputStream(tempFile);
            byte[] b = new byte[PDF_PREAMBLE.length()];
            in.read(b);
            Assert.assertEquals(PDF_PREAMBLE, new String(b));

            // afterprint must STILL not be called after onWrite completes (session is still
            // active).
            String afterCountAfterWrite =
                    JavaScriptUtils.executeJavaScriptAndWaitForResult(
                            webContents, "window.afterPrintCount.toString()");
            Assert.assertEquals("\"0\"", afterCountAfterWrite);
        } finally {
            if (in != null) in.close();

            // Stage 3: onFinish (FinishPrint) - should dispatch afterprint and restore normal mode.
            callFinishOnUiThread(printingController);
            fileDescriptor.close();
            TestFileUtil.deleteFile(tempFile.getAbsolutePath());
        }

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        String count =
                                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                        webContents, "window.afterPrintCount.toString()");
                        return "\"1\"".equals(count);
                    } catch (TimeoutException e) {
                        return false;
                    }
                },
                "afterprint event was not dispatched during onFinish");
    }

    /**
     * Test that multiple onLayout and onWrite cycles (e.g. user toggles orientation in the OS print
     * dialog) do not re-dispatch beforeprint or prematurely dispatch afterprint.
     */
    @Test
    @MediumTest
    @Feature({"Printing"})
    public void testThreeStageAsyncPrintLifecycleMultipleLayoutCycles() throws Throwable {
        final String html =
                "<html><head><script>window.beforePrintCount = 0;window.afterPrintCount ="
                    + " 0;window.addEventListener('beforeprint', () => { window.beforePrintCount++;"
                    + " });window.addEventListener('afterprint', () => { window.afterPrintCount++;"
                    + " });</script></head><body>Multiple Layout Test</body></html>";

        WebPageStation page = mActivityTestRule.startOnUrl(UrlUtils.encodeHtmlDataUri(html));
        Tab tab = page.getTab();
        WebContents webContents = tab.getWebContents();
        final PrintingControllerImpl printingController = createControllerOnUiThread();

        startControllerOnUiThread(printingController, tab);

        // Stage 1: onStart (InitiatePrint)
        callStartOnUiThread(printingController);

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        String count =
                                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                        webContents, "window.beforePrintCount.toString()");
                        return "\"1\"".equals(count);
                    } catch (TimeoutException e) {
                        return false;
                    }
                },
                "beforeprint event was not dispatched during onStart");

        // Cycle 1: onLayout -> onWrite
        final File tempFile1 = File.createTempFile(TEMP_FILE_NAME, TEMP_FILE_EXTENSION);
        final ParcelFileDescriptor fd1 =
                ParcelFileDescriptor.open(tempFile1, ParcelFileDescriptor.MODE_READ_WRITE);
        final WaitForOnWriteHelper onWrite1Completed = new WaitForOnWriteHelper();

        final WriteResultCallbackWrapper writeResultCallback1 =
                new WriteResultCallbackWrapperMock() {
                    @Override
                    public void onWriteFinished(PageRange[] pages) {
                        onWrite1Completed.notifyCalled();
                    }
                };

        final LayoutResultCallbackWrapper layoutResultCallback1 =
                new LayoutResultCallbackWrapperMock() {
                    @Override
                    public void onLayoutFinished(PrintDocumentInfo info, boolean changed) {
                        printingController.onWrite(
                                new PageRange[] {PageRange.ALL_PAGES},
                                fd1,
                                new CancellationSignal(),
                                writeResultCallback1);
                    }
                };

        callLayoutOnUiThread(
                printingController,
                null,
                createPlaceholderPrintAttributes(),
                layoutResultCallback1);

        onWrite1Completed.waitForCallback("First onWrite callback never completed.");
        Assert.assertTrue(tempFile1.length() > 0);
        fd1.close();
        TestFileUtil.deleteFile(tempFile1.getAbsolutePath());

        // Verify beforeprint is still 1 and afterprint is still 0
        String beforeCountAfterCycle1 =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        webContents, "window.beforePrintCount.toString()");
        Assert.assertEquals("\"1\"", beforeCountAfterCycle1);
        String afterCountAfterCycle1 =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        webContents, "window.afterPrintCount.toString()");
        Assert.assertEquals("\"0\"", afterCountAfterCycle1);

        // Cycle 2: second onLayout -> onWrite (simulating layout/attribute change)
        final File tempFile2 = File.createTempFile(TEMP_FILE_NAME, TEMP_FILE_EXTENSION);
        final ParcelFileDescriptor fd2 =
                ParcelFileDescriptor.open(tempFile2, ParcelFileDescriptor.MODE_READ_WRITE);
        final WaitForOnWriteHelper onWrite2Completed = new WaitForOnWriteHelper();

        final WriteResultCallbackWrapper writeResultCallback2 =
                new WriteResultCallbackWrapperMock() {
                    @Override
                    public void onWriteFinished(PageRange[] pages) {
                        onWrite2Completed.notifyCalled();
                    }
                };

        final LayoutResultCallbackWrapper layoutResultCallback2 =
                new LayoutResultCallbackWrapperMock() {
                    @Override
                    public void onLayoutFinished(PrintDocumentInfo info, boolean changed) {
                        printingController.onWrite(
                                new PageRange[] {PageRange.ALL_PAGES},
                                fd2,
                                new CancellationSignal(),
                                writeResultCallback2);
                    }
                };

        callLayoutOnUiThread(
                printingController,
                createPlaceholderPrintAttributes(),
                createPlaceholderPrintAttributes(),
                layoutResultCallback2);

        try {
            onWrite2Completed.waitForCallback("Second onWrite callback never completed.");
            Assert.assertTrue(tempFile2.length() > 0);

            // Verify counts remain unchanged (beforeprint=1, afterprint=0)
            String beforeCountAfterCycle2 =
                    JavaScriptUtils.executeJavaScriptAndWaitForResult(
                            webContents, "window.beforePrintCount.toString()");
            Assert.assertEquals("\"1\"", beforeCountAfterCycle2);
            String afterCountAfterCycle2 =
                    JavaScriptUtils.executeJavaScriptAndWaitForResult(
                            webContents, "window.afterPrintCount.toString()");
            Assert.assertEquals("\"0\"", afterCountAfterCycle2);
        } finally {
            // Stage 3: onFinish (FinishPrint)
            callFinishOnUiThread(printingController);
            fd2.close();
            TestFileUtil.deleteFile(tempFile2.getAbsolutePath());
        }

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        String count =
                                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                        webContents, "window.afterPrintCount.toString()");
                        return "\"1\"".equals(count);
                    } catch (TimeoutException e) {
                        return false;
                    }
                },
                "afterprint event was not dispatched during onFinish");
    }
}
