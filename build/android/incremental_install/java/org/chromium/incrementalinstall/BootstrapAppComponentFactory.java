// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.incrementalinstall;

import android.app.Activity;
import android.app.AppComponentFactory;
import android.app.Application;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.ContentProvider;
import android.content.Intent;
import android.content.pm.ApplicationInfo;

/** Delegates to the real AppComponentFactory. */
public class BootstrapAppComponentFactory extends AppComponentFactory {
    static AppComponentFactory sDelegate;

    @Override
    public ClassLoader instantiateClassLoader(ClassLoader cl, ApplicationInfo aInfo) {
        if (sDelegate != null) {
            throw new AssertionError("Not expecting this call since delegate is set after this.");
        }
        return super.instantiateClassLoader(cl, aInfo);
    }

    @Override
    public Application instantiateApplication(ClassLoader cl, String className)
            throws InstantiationException, IllegalAccessException, ClassNotFoundException {
        if (sDelegate != null) {
            return sDelegate.instantiateApplication(cl, className);
        }
        return super.instantiateApplication(cl, className);
    }

    @Override
    public ContentProvider instantiateProvider(ClassLoader cl, String className)
            throws ClassNotFoundException, IllegalAccessException, InstantiationException {
        if (sDelegate != null) {
            return sDelegate.instantiateProvider(cl, className);
        }
        return super.instantiateProvider(cl, className);
    }

    @Override
    public Activity instantiateActivity(ClassLoader cl, String className, Intent intent)
            throws ClassNotFoundException, IllegalAccessException, InstantiationException {
        if (sDelegate != null) {
            return sDelegate.instantiateActivity(cl, className, intent);
        }
        return super.instantiateActivity(cl, className, intent);
    }

    @Override
    public BroadcastReceiver instantiateReceiver(ClassLoader cl, String className, Intent intent)
            throws ClassNotFoundException, IllegalAccessException, InstantiationException {
        if (sDelegate != null) {
            return sDelegate.instantiateReceiver(cl, className, intent);
        }
        return super.instantiateReceiver(cl, className, intent);
    }

    @Override
    public Service instantiateService(ClassLoader cl, String className, Intent intent)
            throws InstantiationException, IllegalAccessException, ClassNotFoundException {
        if (sDelegate != null) {
            return sDelegate.instantiateService(cl, className, intent);
        }
        return super.instantiateService(cl, className, intent);
    }
}
