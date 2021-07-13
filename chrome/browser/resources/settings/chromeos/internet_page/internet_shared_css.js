import '//resources/cr_elements/shared_vars_css.m.js';
import '../../settings_shared_css.js';
const template = document.createElement('template');
template.innerHTML = `
<dom-module id="internet-shared" assetpath="chrome://resources/">{__html_template__}</dom-module>
`;
document.body.appendChild(template.content.cloneNode(true));