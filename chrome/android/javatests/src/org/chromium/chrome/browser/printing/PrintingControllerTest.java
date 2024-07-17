// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.printing;

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

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.TestFileUtil;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.printing.PrintDocumentAdapterWrapper.LayoutResultCallbackWrapper;
import org.chromium.printing.PrintDocumentAdapterWrapper.WriteResultCallbackWrapper;
import org.chromium.printing.PrintManagerDelegate;
import org.chromium.printing.PrintingControllerImpl;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Tests Android printing. TODO(cimamoglu): Add a test with cancellation. TODO(cimamoglu): Add a
 * test with multiple, stacked onLayout/onWrite calls. TODO(cimamoglu): Add a test which emulates
 * Chromium failing to generate a PDF.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PrintingControllerTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private static final String TEMP_FILE_NAME = "temp_print";
    private static final String TEMP_FILE_EXTENSION = ".pdf";
    private static final String URL =
            UrlUtils.encodeHtmlDataUri("<html><head></head><body>foo</body></html>");
    private static final String PDF_PREAMBLE = "%PDF-1";
    private static final long TEST_TIMEOUT = 20000L;

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
        private File mTempFile;
        private ParcelFileDescriptor mFileDescriptor;

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
        private WaitForOnWriteHelper mWaitForOnWrite;

        public PrintingControllerImplPdfWritingDone(WaitForOnWriteHelper waitForOnWrite) {
            mWaitForOnWrite = waitForOnWrite;
            sInstance = this;
        }

        @Override
        public void pdfWritingDone(int pageCount) {
            mWaitForOnWrite.notifyCalled();
        }
    }

    /**
     * Test a basic printing flow by emulating the corresponding system calls to the printing
     * controller: onStart, onLayout, onWrite, onFinish. Each one is called once, and in this order,
     * in the UI thread.
     */
    @Test
    @LargeTest
    @Feature({"Printing"})
    public void testNormalPrintingFlow() throws Throwable {
        mActivityTestRule.startMainActivityWithURL(URL);
        final Tab currentTab = mActivityTestRule.getActivity().getActivityTab();

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
                printingController, null, createDummyPrintAttributes(), layoutResultCallback);

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

    /**
     * Test for http://crbug.com/528909 Simulating while a printing job is triggered and about to
     * call Android framework to show UI, the corresponding tab is closed, this behaviour is mostly
     * from JavaScript code. Make sure we don't crash and won't call into framework.
     */
    @Test
    @MediumTest
    @Feature({"Printing"})
    public void testPrintCloseWindowBeforeStart() {
        mActivityTestRule.startMainActivityWithURL(URL);
        final Tab currentTab = mActivityTestRule.getActivity().getActivityTab();
        final PrintingControllerImpl printingController = createControllerOnUiThread();
        final PrintManagerDelegate mockPrintManagerDelegate =
                mockPrintManagerDelegate(() -> Assert.fail("Shouldn't start a printing job."));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    printingController.setPendingPrint(
                            new TabPrinter(currentTab), mockPrintManagerDelegate, -1, -1);
                    TabModelUtils.closeCurrentTab(
                            mActivityTestRule.getActivity().getCurrentTabModel());
                    Assert.assertFalse(
                            "currentTab should be closed already.", currentTab.isInitialized());
                    printingController.startPendingPrint();
                });
    }

    /**
     * Test for http://crbug.com/528909 Simulating while a printing job is triggered and printing UI
     * is showing, the corresponding tab is closed, this behaviour is mostly from JavaScript code.
     * Make sure we don't crash and let framework notify user that we can't perform printing job.
     */
    @Test
    @LargeTest
    @Feature({"Printing"})
    public void testPrintCloseWindowBeforeOnWrite() throws Throwable {
        mActivityTestRule.startMainActivityWithURL(URL);
        final Tab currentTab = mActivityTestRule.getActivity().getActivityTab();
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
                printingController, null, createDummyPrintAttributes(), layoutResultCallback);

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
                        TabModelUtils.closeCurrentTab(
                                mActivityTestRule.getActivity().getCurrentTabModel());
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
     * Test for http://crbug.com/863297 This bug shows Android printing framework could call
     * |PrintDocumentAdapter.onFinish()| before one of |WriteResultCallback.onWrite{Cancelled,
     * Failed, Finished}()| get called. Crash test, pass if there is no crash.
     */
    @Test
    @MediumTest
    @Feature({"Printing"})
    public void testCancelPrintBeforeWriteResultCallbacks() throws Throwable {
        mActivityTestRule.startMainActivityWithURL(URL);

        final WaitForOnWriteHelper onWriteHelper = new WaitForOnWriteHelper();
        final Tab currentTab = mActivityTestRule.getActivity().getActivityTab();
        final PrintingControllerImpl printingController =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new PrintingControllerImplPdfWritingDone(onWriteHelper));

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
                    printingController, null, createDummyPrintAttributes(), layoutResultCallback);
            onWriteHelper.waitForCallback("pdfWritingDone never called");
            callFinishOnUiThread(printingController);
        }
    }

    /**
     * Regresstion test for crbug.com/974581. In some cases, native printing code will fail without
     * starting a printing task in Java side. pdfWritingDone() will be called with |pageCount| = 0
     * in this case. We don't need to do anything for this in Java side for now.
     */
    @Test
    @SmallTest
    @Feature({"Printing"})
    public void testPdfWritingDoneCalledWithoutInitailizePrintingTask() {
        mActivityTestRule.startMainActivityWithURL(URL);
        final PrintingControllerImpl controller = createControllerOnUiThread();

        // Calling pdfWritingDone() with |pageCount| = 0 before onWrite() was called. It shouldn't
        // crash.
        ThreadUtils.runOnUiThreadBlocking(() -> controller.pdfWritingDone(0));
    }

    private PrintingControllerImpl createControllerOnUiThread() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> (PrintingControllerImpl) PrintingControllerImpl.getInstance());
    }

    private PrintAttributes createDummyPrintAttributes() {
        return new PrintAttributes.Builder()
                .setMediaSize(PrintAttributes.MediaSize.ISO_A4)
                .setResolution(new PrintAttributes.Resolution("foo", "bar", 300, 300))
                .setMinMargins(PrintAttributes.Margins.NO_MARGINS)
                .build();
    }

    private PrintManagerDelegate mockPrintManagerDelegate(final Runnable r) {
        return new PrintManagerDelegate() {
            @Override
            public void print(
                    String printJobName,
                    PrintDocumentAdapter documentAdapter,
                    PrintAttributes attributes) {
                if (r != null) r.run();
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
}
