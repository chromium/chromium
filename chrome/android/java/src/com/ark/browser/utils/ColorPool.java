package com.ark.browser.utils;

import android.graphics.Color;

import androidx.annotation.ColorInt;

public class ColorPool {

    private static final int[] COLORS = {
            Color.parseColor("#3e53f1"),
            Color.parseColor("#097FFB"),
            Color.parseColor("#23BFD6"),
            Color.parseColor("#00CBB5"),
            Color.parseColor("#0AD270"),
            Color.parseColor("#FFC700"),
            Color.parseColor("#FF9623"),
            Color.parseColor("#F02867"),
            Color.parseColor("#C052D7"),
            Color.parseColor("#6442B3"),
            Color.parseColor("#8C6A68"),
            Color.parseColor("#C41442")
    };

    @ColorInt
    public static int getColor(int index) {
        int i = index % COLORS.length;
        return COLORS[i];
    }


}

