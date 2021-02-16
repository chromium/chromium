(function() {
  const metaTags = document.getElementsByTagName('meta');
  let output = {};
  for (let i = 0; i < metaTags.length; i++) {
    let curMeta = metaTags[i];
    let name = curMeta.getAttribute("property");
    let value = curMeta.getAttribute("content");
    if (!name || !value) continue;
    if (name.startsWith("og:")) {
      output[name.substring(3)] = value;
    }
  }
  console.log(output);
  console.log(JSON.stringify(output));
  return JSON.stringify(output);
})();
