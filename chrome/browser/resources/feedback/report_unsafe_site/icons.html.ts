// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_iconset.js';

import {getTrustedHTML} from '//resources/js/static_types.js';

const div = document.createElement('div');
div.innerHTML = getTrustedHTML`
<cr-iconset name="report_unsafe_site" size="20">
  <svg>
    <defs>
      <!--
      Keep these in sorted order by id="".
      -->
      <g id="visibility-off">
        <path fill-rule="evenodd" clip-rule="evenodd" d="M17.3897 18.0247L1.9895 2.62442L2.01413 2.59979L2.00015 2.58582L0.585938 4.00003L3.29316 6.70725C2.42105 7.5893 1.65672 8.6869 1.00015 10C2.66682 14 5.66682 16 10.0002 16C10.8372 16 11.6245 15.9254 12.3621 15.7761L16.0002 19.4142L17.3897 18.0247ZM3.23605 10.0696C3.68277 9.31012 4.17153 8.66159 4.70605 8.12014L7.03328 10.4474C7.22745 11.7461 8.25408 12.7727 9.55281 12.9669L10.5721 13.9862C10.3852 13.9954 10.1946 14 10.0002 14C6.7924 14 4.62265 12.7465 3.23605 10.0696Z"></path>
        <path d="M16.1839 13.9904C17.3593 13.0152 18.298 11.6851 19.0002 10C17.0002 6.00003 14.0002 4.00003 10.0002 4.00003C8.81854 4.00003 7.7242 4.17455 6.71711 4.5236L8.34623 6.15272C8.86951 6.05045 9.42035 6.00003 10.0002 6.00003C12.9375 6.00003 15.1317 7.29405 16.7643 10.0696C16.2217 11.117 15.5593 11.9465 14.7617 12.5682L16.1839 13.9904Z"></path>
        <path d="M12.9133 10.7198L9.28043 7.08692C9.511 7.03014 9.75206 7.00003 10.0002 7.00003C11.657 7.00003 13.0002 8.34317 13.0002 10C13.0002 10.2481 12.97 10.4892 12.9133 10.7198Z"></path>
      </g>
  </svg>
</cr-iconset>`;

const iconsets = div.querySelectorAll('cr-iconset');
for (const iconset of iconsets) {
  document.head.appendChild(iconset);
}
