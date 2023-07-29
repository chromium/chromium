package com.ark.browser.ui.widget.homepage;

import android.view.View;
import android.view.ViewGroup;

import com.ark.browser.tab.core.ITab;

public interface Adapter {

    View onCreateViewHolder(ViewGroup parent, ITab tab, int position);

    void onBindViewHolder(View itemView, ITab tab, int position);

}
