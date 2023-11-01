// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A collection of document fragments to allow for easier test
 * creation.
 */

const Documents = {
  application: `<div role="application" id="application">application</div>`,
  banner: `<div role="banner" id="banner">banner</div>`,
  button: `<button id="button">button</button>`,
  checkbox: `<input type="checkbox" id="checkbox"></input>
      <label for="checkbox">checkbox</label>`,
  color: `<input type="color" id="color"></input>
      <label for="color">color</label>`,
  complementary:
      `<div role="complementary" id="complementary">Complementary</div>`,
  form: `<form aria-label="form" id="form"></form>`,
  grid: `<div role="grid" id="grid">grid</div>`,
  header: `<h1 id="header">header</header>`,
  link: `<a href="#link" id="link">link</a>`,
  main: `<div role="main" id="main">main</div>`,
  navigation: `<nav id="navigation">navigation</nav>`,
  region: `<div role="region" id="region">region</div>`,
  search: `<input type="text" role="search" id="search"></input>
      <label for="search">search</label>`,
  slider: `<input type="range" id="slider"></input>
      <label for="slider">slider</label>`,
  switch: `<button role="switch" id="switch" aria-checked=true>switch</button>
    <script>
        const switchElement = document.getElementById("switch");
        switchElement.onclick =
            () => switchElement.ariaChecked = !switchElement.ariaChecked;
    </script>`,
  tab: `<div role="tab" id="tab">tab</div>`,
  table: `<table id="table" aria-label="table"></table>`,
  textarea: `<textarea aria-label="textarea" id="textarea"></textarea>`,
  textInput:
      `<input type="text" aria-label="textInput" id="textInput"></input>`,
  tree: `<div role="tree" id="tree">tree</div>`,

  // From w3 WAI ARIA examples:
  customTabList: `<div role="tablist" id="customTabList">
         <button id="tab1" role="tab" aria-selected="true"
                 aria-controls="panel1">
           tab1
         </button>
         <button id="tab2" role="tab" aria-selected="false"
                 aria-controls="panel2" tabindex=-1>
           tab2
         </button>
       </div>
       <div id="panel1" role="tabpanel" aria-labelledby="tab1">
         panel1
       </div>
       <div id="panel2" role="tabpanel" aria-labelledby="tab2" hidden>
         panel2
       </div>
       <script>
         const $ = id => document.getElementById(id);
         const tablistNode = $('tablist');
         const tabs = ['tab1', 'tab2'].map($);
         const tabPanels = ['panel1', 'panel2'].map($);
         let focused = 0;

         const select = tab => {
           for (let i = 0; i < tabs.length; i++) {
             if (tab === tabs[i]) {
               tab.setAttribute('aria-selected', 'true');
               tab.removeAttribute('tabindex');
               tabPanels[i].removeAttribute('hidden');
               focused = i;
             } else {
               tabs[i].setAttribute('aria-selected', 'false');
               tabs[i].tabIndex = -1;
               tabPanels[i].setAttribute('hidden', 'true');
             }
           }
         };

         const keydown = event => {
           switch(event.key) {
             case 'ArrowLeft':
               tabs[focused === 1 ? 0 : 1].focus();
               break;
             case 'ArrowRight':
               tabs[focused === 1 ? 0 : 1].focus();
               break;
           }
           if (['ArrowLeft', 'ArrowRight'].some(k => event.key === k)) {
             event.stopPropagation();
             event.preventDefault();
           }
         };
         tabs.map(tab => tab.addEventListener('keydown', keydown));

         const click = event => select(event.currentTarget);
         tabs.map(tab => tab.addEventListener('click', click));
       </script>`,
};
