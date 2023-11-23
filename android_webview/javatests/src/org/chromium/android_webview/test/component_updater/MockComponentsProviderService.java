// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.component_updater;

import android.app.Service;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.ResultReceiver;

import org.chromium.android_webview.services.ComponentsProviderService;
import org.chromium.components.component_updater.IComponentsProviderService;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.HashMap;

/**
 * Mock {@link org.chrmoium.android_webview.services.ComponentsProviderService} for tests.
 *
 * It accepts a list of files paths in the onBind {@link Intent} that it opens as a result for the
 * incoming componentId.
 */
public class MockComponentsProviderService extends Service {
    @Override
    public IBinder onBind(Intent intent) {
        return new IComponentsProviderService.Stub() {
            @Override
            public void getFilesForComponent(String componentId, ResultReceiver resultReceiver) {
                CharSequence[] filePaths;
                if ((filePaths = intent.getCharSequenceArrayExtra(componentId)) != null) {
                    Bundle resultBundle = new Bundle();
                    HashMap<String, ParcelFileDescriptor> resultMap = new HashMap<>();
                    for (CharSequence filePath : filePaths) {
                        File file = new File(filePath.toString());
                        try {
                            resultMap.put(
                                    file.getName(),
                                    ParcelFileDescriptor.open(
                                            file, ParcelFileDescriptor.MODE_READ_ONLY));
                        } catch (FileNotFoundException exception) {
                            throw new RuntimeException(exception);
                        }
                    }
                    resultBundle.putSerializable(ComponentsProviderService.KEY_RESULT, resultMap);
                    resultReceiver.send(ComponentsProviderService.RESULT_OK, resultBundle);
                } else {
                    resultReceiver.send(ComponentsProviderService.RESULT_FAILED, null);
                }
            }
        };
    }
}
