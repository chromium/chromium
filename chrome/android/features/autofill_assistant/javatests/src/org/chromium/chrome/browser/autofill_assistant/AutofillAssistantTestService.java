// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import com.google.protobuf.ByteString;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionsResponseProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportsScriptResponseProto;

import java.util.List;

/**
 * Test service which serves preconfigured scripts and actions.
 */
@JNINamespace("autofill_assistant")
public class AutofillAssistantTestService
        implements AutofillAssistantService,
                   AutofillAssistantServiceInjector.NativeServiceProvider {
    private final List<AutofillAssistantTestScript> mScripts;

    AutofillAssistantTestService(List<AutofillAssistantTestScript> scripts) {
        mScripts = scripts;
    }

    /**
     * Asks the client to inject this service upon startup. Must be called prior to client startup
     * in order to take effect!
     */
    void scheduleForInjection() {
        AutofillAssistantServiceInjector.setServiceToInject(this);
    }

    @Override
    public long createNativeService() {
        // Ask native to create and return a wrapper around |this|. The wrapper will be injected
        // upon startup, at which point the native controller will take ownership of the wrapper.
        return AutofillAssistantTestServiceJni.get().javaServiceCreate(this);
    }

    /** @see AutofillAssistantService#getScriptsForUrl(String) */
    @Override
    public SupportsScriptResponseProto getScriptsForUrl(String url) {
        SupportsScriptResponseProto.Builder builder = SupportsScriptResponseProto.newBuilder();
        for (AutofillAssistantTestScript script : mScripts) {
            builder.addScripts(script.getSupportedScript());
        }
        return builder.build();
    }

    /** @see AutofillAssistantService#getActions(String, String, byte[], byte[]) */
    @Override
    public ActionsResponseProto getActions(
            String scriptPath, String url, byte[] globalPayload, byte[] scriptPayload) {
        for (AutofillAssistantTestScript script : mScripts) {
            if (script.getSupportedScript().getPath().compareTo(scriptPath) != 0) {
                continue;
            }

            return ActionsResponseProto.newBuilder()
                    .addAllActions(script.getActions())
                    .setGlobalPayload(ByteString.copyFrom(globalPayload))
                    .setScriptPayload(ByteString.copyFrom(scriptPayload))
                    .build();
        }

        // Actions requested for non-existing script: return empty response.
        return ActionsResponseProto.getDefaultInstance();
    }

    /** @see AutofillAssistantService#getNextActions(byte[], byte[]) */
    @Override
    public ActionsResponseProto getNextActions(byte[] globalPayload, byte[] scriptPayload) {
        return ActionsResponseProto.getDefaultInstance();
    }

    @CalledByNative
    private byte[] getScriptsForUrlNative(String url) {
        return getScriptsForUrl(url).toByteArray();
    }

    @CalledByNative
    private byte[] getActionsNative(
            String scriptPath, String url, byte[] globalPayload, byte[] scriptPayload) {
        return getActions(scriptPath, url, globalPayload, scriptPayload).toByteArray();
    }

    @CalledByNative
    private byte[] getNextActionsNative(byte[] globalPayload, byte[] scriptPayload) {
        return getNextActions(globalPayload, scriptPayload).toByteArray();
    }

    @NativeMethods
    interface Natives {
        long javaServiceCreate(AutofillAssistantTestService service);
    }
}
