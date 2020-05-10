$progressPreference = 'silentlyContinue'

rm -Recurse -Force out2
mkdir -Force out2

cp -Force package.json ./out2/
cp -Force index.js ./out2/
cp -Force child.js ./out2/
cp -Force example.html ./out2/
cp -Force metachromium.cmd ./out2/

wget "https://nodejs.org/dist/v14.2.0/node-v14.2.0-win-x64.zip" -OutFile node.zip
7z x node.zip -aoa
rm -Force node.zip
rm -Recurse -Force node
mv node-v14.2.0-win-x64 out2/node

cp -Force ./out/Release/chrome.packed.7z metachromium.7z
7z x metachromium.7z -aoa
rm -Force metachromium.7z
mv chrome.7z out2
cd out2
7z x chrome.7z -aoa
rm -Force chrome.7z

.\node\node.exe .\node\node_modules\npm\bin\npm-cli.js install

cd ..