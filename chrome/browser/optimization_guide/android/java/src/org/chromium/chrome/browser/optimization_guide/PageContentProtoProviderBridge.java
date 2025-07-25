// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.optimization_guide;

import com.google.protobuf.InvalidProtocolBufferException;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.optimization_guide.features.proto.CommonFeatureDataProto.AnnotatedPageContent;
import org.chromium.content_public.browser.WebContents;

import java.util.Optional;

@JNINamespace("optimization_guide::android")
@NullMarked
public class PageContentProtoProviderBridge {

    public static void getAiPageContent(
            WebContents webContents, Callback<Optional<AnnotatedPageContent>> callback) {
        PageContentProtoProviderBridgeJni.get()
                .getAiPageContent(
                        webContents,
                        new Callback<byte[]>() {
                            @Override
                            public void onResult(byte[] result) {
                                if (result.length == 0) {
                                    callback.onResult(Optional.empty());
                                    return;
                                }

                                try {
                                    AnnotatedPageContent proto =
                                            AnnotatedPageContent.parseFrom(result);
                                    callback.onResult(Optional.of(proto));
                                } catch (InvalidProtocolBufferException e) {
                                    callback.onResult(Optional.empty());
                                }
                            }
                        });
    }

    @NativeMethods
    public interface Natives {
        void getAiPageContent(
                @JniType("content::WebContents*") WebContents webContents,
                Callback<byte[]> callback);
    }
}
