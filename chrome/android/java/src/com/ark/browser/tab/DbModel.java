package com.ark.browser.tab;

import com.ark.browser.utils.ThreadPool;

public abstract class DbModel {

    public abstract void save();

    public void update() {
        save();
    }

    public void delete() {
        ThreadPool.executeIO(this::deleteSync);
    }

    public abstract void deleteSync();


}
