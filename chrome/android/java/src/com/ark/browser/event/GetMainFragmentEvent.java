package com.ark.browser.event;

import com.ark.browser.ui.fragment.ArkMainFragment;

import org.chromium.base.Callback;

public class GetMainFragmentEvent extends BaseCallbackEvent<ArkMainFragment> {

    protected GetMainFragmentEvent(Callback<ArkMainFragment> callback) {
        super(callback);
    }

    public static void post(Callback<ArkMainFragment> callback) {
        new GetMainFragmentEvent(callback).post();
    }

}
