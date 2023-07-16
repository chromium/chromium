package com.ark.browser.event;

import com.zpj.bus.ZBus;

import org.chromium.base.Callback;

public class BaseCallbackEvent<T> {

    private final Callback<T> callback;

    protected BaseCallbackEvent(Callback<T> callback) {
        this.callback = callback;
    }

    public void onResult(T result) {
        callback.onResult(result);
    }

    public void post() {
        ZBus.post(this);
    }

}
