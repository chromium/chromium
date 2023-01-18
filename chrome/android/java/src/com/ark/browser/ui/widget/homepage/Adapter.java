package com.ark.browser.ui.widget.homepage;

import android.view.View;
import android.view.ViewGroup;

public interface Adapter {

    View onCreateViewHolder(ViewGroup parent, int position);

    void onBindViewHolder(View itemView, int position);

    int getCount();

    int getPosition();

    boolean onSwipe(int position);

    boolean isLocked(int position);

}
