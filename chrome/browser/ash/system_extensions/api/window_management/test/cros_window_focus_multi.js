// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js', 'cros_window_test_utils.js');

promise_test(async (t) => {
  // TODO(b/245442671): Remove waiting for event dispatch once events are
  // synchronised.
  const eventWatcher =
      new EventWatcher(t, chromeos.windowManagement, ['windowopened']);

  // Open a browser window that takes focus.
  await testHelper.openBrowserWindow();

  await eventWatcher.wait_for(['windowopened']);

  // async window retriever with stable window ordering after first retrieval.
  // TODO(b/242264908): Remove once the order of windows is guaranteed.
  let getWindows;

  {
    let [first_window, second_window] =
        await chromeos.windowManagement.getWindows();
    getWindows = async function() {
      let [first_returned_window, second_returned_window] =
          await chromeos.windowManagement.getWindows();
      assert_equals(first_window.id, first_returned_window.id);
      assert_equals(second_window.id, second_returned_window.id);
      return [first_returned_window, second_returned_window];
    };
  }

  {
    let [first_window, second_window] = await getWindows();
    // When focusing 1st window, it should have sole focus.
    await first_window.focus();
  }

  {
    let [first_window, second_window] = await getWindows();
    assert_true(first_window.isFocused);
    assert_false(second_window.isFocused);
  }

  {
    let [first_window, second_window] = await getWindows();
    // When focusing 2nd window, it should have sole focus.
    await second_window.focus();

    [first_window, second_window] = await getWindows();
    assert_false(first_window.isFocused);
    assert_true(second_window.isFocused);
  }

  {
    let [first_window, second_window] = await getWindows();
    // Fullscreening a window does not focus an unfocused window.
    await first_window.setFullscreen(true);

    [first_window, second_window] = await getWindows();
    assert_false(first_window.isFocused);
    assert_true(second_window.isFocused);
  }

  {
    let [first_window, second_window] = await getWindows();
    // We can focus a fullscreen window.
    await first_window.focus();

    [first_window, second_window] = await getWindows();
    assert_true(first_window.isFocused);
    assert_false(second_window.isFocused);
  }

  {
    let [first_window, second_window] = await getWindows();
    // We can focus another window on top of a fullscreen window.
    await second_window.focus();

    [first_window, second_window] = await getWindows();
    assert_false(first_window.isFocused);
    assert_true(second_window.isFocused);
  }

  {
    let [first_window, second_window] = await getWindows();
    // Minimizing focused window should pass focus to next window.
    await second_window.minimize();

    [first_window, second_window] = await getWindows();
    assert_true(first_window.isFocused);
    assert_false(second_window.isFocused);
  }

  {
    let [first_window, second_window] = await getWindows();
    // Minimizing remaining window should lose focus.
    await first_window.minimize();

    [first_window, second_window] = await getWindows();
    assert_false(first_window.isFocused);
    assert_false(second_window.isFocused);
  }
});
