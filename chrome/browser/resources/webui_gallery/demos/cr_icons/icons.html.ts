// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_iconset.js';

import {getTrustedHTML} from '//resources/js/static_types.js';

const div = document.createElement('div');
if (document.documentElement.hasAttribute('webui-rounded-icons')) {
  div.innerHTML = getTrustedHTML`
<cr-iconset name="desserts">
  <svg>
    <defs>
      <g id="cake"><path d="M4.8 21.602c-.34 0-.624-.118-.855-.348a1.16 1.16 0 0 1-.343-.856v-4.796c0-.497.175-.922.527-1.274a1.735 1.735 0 0 1 1.27-.527H6V9.6c0-.496.176-.921.527-1.273a1.744 1.744 0 0 1 1.274-.527h3.3V6.449a2.783 2.783 0 0 1-.664-.625c-.16-.215-.238-.515-.238-.894 0-.235.043-.457.125-.668.086-.207.215-.403.399-.582l.953-.953.324-.153c.066 0 .176.043.324.125l.973.973c.183.183.316.383.39.59.075.21.114.433.114.668 0 .379-.078.68-.239.894-.16.219-.378.426-.664.625v1.352H16.2c.496 0 .918.176 1.274.527.351.352.527.777.527 1.274V13.8h.602c.492 0 .918.176 1.27.527.35.352.526.777.526 1.274v4.796c0 .34-.113.625-.343.856-.23.23-.516.348-.856.348Zm3-7.801h8.4V9.6H7.8Zm-2.402 6h13.204v-4.2H5.398Zm2.403-6h8.398Zm-2.403 6h13.204Zm13.204-6H5.398Zm0 0"></path></g>
    </defs>
  </svg>
</cr-iconset>`;
} else {
  div.innerHTML = getTrustedHTML`
<cr-iconset name="desserts">
  <svg>
    <defs>
      <g id="cake"><path d="M4 22q-.425 0-.712-.288Q3 21.425 3 21v-5q0-.825.587-1.413Q4.175 14 5 14v-4q0-.825.588-1.413Q6.175 8 7 8h4V6.55q-.45-.3-.725-.725Q10 5.4 10 4.8q0-.375.15-.738.15-.362.45-.662L12 2l1.4 1.4q.3.3.45.662.15.363.15.738 0 .6-.275 1.025-.275.425-.725.725V8h4q.825 0 1.413.587Q19 9.175 19 10v4q.825 0 1.413.587Q21 15.175 21 16v5q0 .425-.288.712Q20.425 22 20 22Zm3-8h10v-4H7Zm-2 6h14v-4H5Zm2-6h10Zm-2 6h14Zm14-6H5h14Z"></g>
    </defs>
  </svg>
</cr-iconset>`;
}

const iconsets = div.querySelectorAll('cr-iconset');
for (const iconset of iconsets) {
  document.head.appendChild(iconset);
}
