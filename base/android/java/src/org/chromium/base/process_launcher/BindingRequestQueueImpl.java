// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.content.Context;
import android.content.ServiceConnection;
import android.os.Build;

import androidx.annotation.RequiresApi;

import org.chromium.base.BaseFeatureList;
import org.chromium.base.BindingRequestQueue;
import org.chromium.base.ContextUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

import java.util.LinkedHashMap;

/**
 * A queue of binding requests. This is used to batch up binding requests to improve performance.
 */
@RequiresApi(Build.VERSION_CODES.CINNAMON_BUN)
@NullMarked
class BindingRequestQueueImpl implements BindingRequestQueue {
    private final int mBatchSize;
    // LinkedHashMap is used to preserve the order of the requests.
    private final LinkedHashMap<ServiceConnection, Context.UpdateBindingParams>
            mBindingRequestQueue;
    private long mFirstRequestTimeMillis;

    private BindingRequestQueueImpl() {
        mBatchSize = BaseFeatureList.sRebindServiceBatchApiBatchSize.getValue();
        mBindingRequestQueue = new LinkedHashMap<>(mBatchSize);
    }

    private static class SingletonHelper {
        private static final BindingRequestQueueImpl INSTANCE = new BindingRequestQueueImpl();
    }

    public static BindingRequestQueueImpl getInstance() {
        return SingletonHelper.INSTANCE;
    }

    @Override
    public void rebind(ServiceConnection connection, Context.BindServiceFlags flags) {
        if (mFirstRequestTimeMillis == 0) {
            mFirstRequestTimeMillis = TimeUtils.elapsedRealtimeMillis();
        }
        Context.UpdateBindingParams params = mBindingRequestQueue.remove(connection);
        if (params == null) {
            params = new Context.UpdateBindingParams.Builder(connection, flags).build();
        } else {
            assert params.isRebind();
            params.setRebind(flags);
        }
        mBindingRequestQueue.put(connection, params);

        if (mBindingRequestQueue.size() >= mBatchSize) {
            flush();
        }
    }

    @Override
    public void unbind(ServiceConnection connection) {
        if (mFirstRequestTimeMillis == 0) {
            mFirstRequestTimeMillis = TimeUtils.elapsedRealtimeMillis();
        }
        Context.UpdateBindingParams params = mBindingRequestQueue.get(connection);
        if (params == null) {
            params = new Context.UpdateBindingParams.Builder(connection).build();
            mBindingRequestQueue.put(connection, params);
        } else {
            params.setUnbind();
        }

        if (mBindingRequestQueue.size() >= mBatchSize) {
            flush();
        }
    }

    @Override
    public void flush() {
        if (mBindingRequestQueue.isEmpty()) {
            return;
        }
        int size = mBindingRequestQueue.size();
        TraceEvent.begin("BindingRequestQueue.flush", size);
        ContextUtils.getApplicationContext().updateServiceBindings(mBindingRequestQueue.values());
        mBindingRequestQueue.clear();
        TraceEvent.end("BindingRequestQueue.flush");
        // We don't log the size histogram if the batch is empty because flush() can be called not
        // only at the end of batch duration but also at other timings (e.g. just before
        // Context.bindService(), launcher thread is idle).
        RecordHistogram.recordCount1000Histogram(
                "Android.ChildProcessBinding.BatchRequestSize", size);
        RecordHistogram.recordTimesHistogram(
                "Android.ChildProcessBinding.BatchRequestDelay",
                TimeUtils.elapsedRealtimeMillis() - mFirstRequestTimeMillis);
        mFirstRequestTimeMillis = 0;
    }
}
