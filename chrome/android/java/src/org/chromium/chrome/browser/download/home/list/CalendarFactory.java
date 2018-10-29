// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import org.chromium.base.task.AsyncTask;

import java.util.Calendar;
import java.util.concurrent.ExecutionException;

/** Helper class to simplify querying for a {@link Calendar} instance. */
public final class CalendarFactory {
    private static final AsyncTask<Calendar> sCalendarBuilder =
            new CalendarBuilder().executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);

    private CalendarFactory() {}

    /**
     *
     * @return A unique {@link Calendar} instance.  This version will (1) not be handed out to any
     *         other caller and (2) will be completely reset.
     */
    public static Calendar get() {
        Calendar calendar = null;
        try {
            calendar = (Calendar) sCalendarBuilder.get().clone();
        } catch (InterruptedException | ExecutionException e) {
            // We've tried our best. If AsyncTask really does not work, we give up. :(
            calendar = Calendar.getInstance();
        }

        calendar.clear();
        return calendar;
    }

    private static class CalendarBuilder extends AsyncTask<Calendar> {
        @Override
        protected Calendar doInBackground() {
            return Calendar.getInstance();
        }
    }
}
