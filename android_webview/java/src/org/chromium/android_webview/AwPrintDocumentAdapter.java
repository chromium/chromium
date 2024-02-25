// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.print.PageRange;
import android.print.PrintAttributes;
import android.print.PrintDocumentAdapter;
import android.print.PrintDocumentInfo;

import java.util.ArrayList;

/**
 * Adapter for printing Webview. This class implements the abstract
 * system class PrintDocumentAdapter and hides all printing details from
 * the developer.
 */
public class AwPrintDocumentAdapter extends PrintDocumentAdapter {

    private AwPdfExporter mPdfExporter;
    private PrintAttributes mAttributes;
    private String mDocumentName;

    /**
     * Constructor.
     * TODO(sgurun) remove in favor of constructor below once the AOSP changes are in.
     *
     * @param pdfExporter The PDF exporter to export the webview contents to a PDF file.
     */
    public AwPrintDocumentAdapter(AwPdfExporter pdfExporter) {
        this(pdfExporter, "default");
    }

    /**
     * Constructor.
     *
     * @param pdfExporter The PDF exporter to export the webview contents to a PDF file.
     * @param documentName  The name of the pdf document.
     */
    public AwPrintDocumentAdapter(AwPdfExporter pdfExporter, String documentName) {
        mPdfExporter = pdfExporter;
        mDocumentName = documentName;
    }

    @Override
    public void onLayout(
            PrintAttributes oldAttributes,
            PrintAttributes newAttributes,
            CancellationSignal cancellationSignal,
            LayoutResultCallback callback,
            Bundle metadata) {
        mAttributes = newAttributes;
        PrintDocumentInfo documentInfo = new PrintDocumentInfo.Builder(mDocumentName).build();
        // TODO(sgurun) once componentization is done, do layout changes and
        // generate PDF here, set the page range information to documentinfo
        // and call onLayoutFinished with true/false depending on whether
        // layout actually changed.
        callback.onLayoutFinished(documentInfo, true);
    }

    @Override
    public void onWrite(
            final PageRange[] pages,
            ParcelFileDescriptor destination,
            CancellationSignal cancellationSignal,
            final WriteResultCallback callback) {
        if (pages == null || pages.length == 0) {
            callback.onWriteFailed(null);
            return;
        }

        mPdfExporter.exportToPdf(
                destination,
                mAttributes,
                normalizeRanges(pages),
                pageCount -> {
                    if (pageCount > 0) {
                        callback.onWriteFinished(validatePageRanges(pages, pageCount));
                    } else {
                        // TODO(sgurun) provide a localized error message
                        callback.onWriteFailed(null);
                    }
                },
                cancellationSignal);
    }

    private static PageRange[] validatePageRanges(PageRange[] pages, int pageCount) {
        if (pages.length == 1 && PageRange.ALL_PAGES.equals(pages[0])) {
            return new PageRange[] {new PageRange(0, pageCount - 1)};
        }
        return pages;
    }

    private static int[] normalizeRanges(final PageRange[] ranges) {
        if (ranges.length == 1 && PageRange.ALL_PAGES.equals(ranges[0])) {
            return new int[0];
        }
        ArrayList<Integer> pages = new ArrayList<Integer>();
        for (PageRange range : ranges) {
            for (int i = range.getStart(); i <= range.getEnd(); ++i) {
                pages.add(i);
            }
        }

        int[] ret = new int[pages.size()];
        for (int i = 0; i < pages.size(); ++i) {
            ret[i] = pages.get(i).intValue();
        }
        return ret;
    }
}
