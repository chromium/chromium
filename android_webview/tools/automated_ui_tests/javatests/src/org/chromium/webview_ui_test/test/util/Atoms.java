// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_ui_test.test.util;

import static androidx.test.espresso.web.model.Atoms.castOrDie;

import androidx.test.espresso.web.model.Atom;
import androidx.test.espresso.web.model.Evaluation;
import androidx.test.espresso.web.model.SimpleAtom;
import androidx.test.espresso.web.model.TransformingAtom;

/** A collection of Javascript Atoms for WebView testing */
public class Atoms {
    /** Click on a select element in HTML */
    public static SimpleAtom webSelect() {
        return new SimpleAtom(
                "function(webElement) {"
                        + "  var e = document.createEvent('MouseEvents');"
                        + "  e.initMouseEvent('mousedown', true, true, window, 0, 0, 0, 0, 0,"
                        + "      false, false, false, false, 0, null);"
                        + "  webElement.dispatchEvent(e);"
                        + "}");
    }

    /** Returns an Atom that represents the text in the select element's currently selected item */
    public static Atom<String> currentSelection() {
        return new TransformingAtom<Evaluation, String>(
                new SimpleAtom("function(e) {return e.options[e.selectedIndex].text;}"),
                castOrDie(String.class));
    }
}
