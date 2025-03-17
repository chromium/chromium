// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.example.autofill_service.fill_service;

import android.os.CancellationSignal;
import android.service.autofill.AutofillService;
import android.service.autofill.FillCallback;
import android.service.autofill.FillRequest;
import android.service.autofill.SaveCallback;
import android.service.autofill.SaveRequest;
import android.widget.Toast;

import org.chromium.example.autofill_service.fill_service.helpers.ResponseHelper;

/**
 * A basic {@link AutofillService} implementation that only shows dynamic-generated datasets and
 * supports inline suggestions.
 */
public class InlineFillService extends AutofillService {

    @Override
    public void onFillRequest(
            FillRequest request, CancellationSignal cancellationSignal, FillCallback callback) {
        callback.onSuccess(ResponseHelper.createSimpleResponse(this, request));
    }

    @Override
    public void onSaveRequest(SaveRequest request, SaveCallback callback) {
        Toast.makeText(this, "InlineFillService doesn't support Save", Toast.LENGTH_LONG).show();
        callback.onSuccess();
    }
}
