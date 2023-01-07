#!/bin/bash

# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if ! which adb &> /dev/null; then
  echo "adb is not in your path, did you run envsetup.sh?"
  exit -1
fi

TMPFILE=$(tempfile)
echo '<body><div>just some text</div></body>' > $TMPFILE
adb push $TMPFILE /data/local/tmp/file.html
rm $TMPFILE
adb shell am start -n com.android.htmlviewer/.HTMLViewerActivity -d \
    "file:///data/local/tmp/file.html" -a VIEW -t "text/html"

sleep 3

echo 'Running test, you should run \
`adb logcat | grep WebViewStartupTimeMillis=` in another shell to see results.'

# Launch webview test shell 100 times
for i in $(seq 1 100); do
  if [[ $(($i % 10)) -eq 0 ]]; then
    echo -n "..$i.."
  fi
  adb shell kill -9 `adb shell ps | grep org.chromium.webview_shell \
      | tr -s " " " " | cut -d" " -f2`
  adb shell am start -n org.chromium.webview_shell/.StartupTimeActivity \
      -a VIEW > /dev/null
  sleep 0.5
done
echo

adb shell rm /data/local/tmp/file.html
