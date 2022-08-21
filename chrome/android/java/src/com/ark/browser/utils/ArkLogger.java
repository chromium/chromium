package com.ark.browser.utils;

import org.chromium.base.Log;

public class ArkLogger {

    public static void d(String tag, String msg) {
        Log.e("Ark_" + tag, msg);
    }



    public static void e(String tag, String msg ) {
        Log.e("Ark_" + tag, msg);
    }

}
