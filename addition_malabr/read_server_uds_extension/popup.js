const readDataBtnEle = document.getElementById('readDataBtn');
const showReadDataResponseEle = document.getElementById('showReadDataResponse');

const sendDataBtnEle = document.getElementById('sendDataBtn');
const sendDataIptEle = document.getElementById('sendDataIpt');
const sendDataErrorEle = document.getElementById('sendDataError');
const showsendDataResponseEle = document.getElementById('showsendDataResponse');

// READ DATA
readDataBtnEle.addEventListener('click', () => {
  // Clear previous
  showReadDataResponseEle.textContent = '';
  showReadDataResponseEle.classList.remove('error');

  chrome.readServerUds.readData((response) => {
    if (chrome.runtime.lastError) {
      showReadDataResponseEle.textContent = 'Native Error: ' + chrome.runtime.lastError.message;
      showReadDataResponseEle.classList.add('error');
      return;
    }

    let parsedResponse;
    try {
      parsedResponse = JSON.parse(response);
    } catch {
      showReadDataResponseEle.textContent = 'Invalid JSON response.';
      showReadDataResponseEle.classList.add('error');
      return;
    }

    if (!parsedResponse.status) {
      showReadDataResponseEle.textContent = parsedResponse.message || 'Server returned an error.';
      showReadDataResponseEle.classList.add('error');
      return;
    }

    showReadDataResponseEle.textContent = parsedResponse.message || 'Success!';
    showReadDataResponseEle.classList.remove('error');
  });
});

// SEND DATA
sendDataBtnEle.addEventListener('click', () => {
  const message = sendDataIptEle.value.trim();

  // Clear previous
  sendDataErrorEle.textContent = '';
  sendDataErrorEle.classList.remove('error');
  showsendDataResponseEle.textContent = '';
  showsendDataResponseEle.classList.remove('error');

  if (!message) {
    sendDataErrorEle.textContent = 'Please enter a message before sending.';
    sendDataErrorEle.classList.add('error');
    return;
  }

  chrome.readServerUds.sendData(message, (response) => {
    if (chrome.runtime.lastError) {
      sendDataErrorEle.textContent = 'Native Error: ' + chrome.runtime.lastError.message;
      sendDataErrorEle.classList.add('error');
      return;
    }

    let parsedResponse;
    try {
      parsedResponse = JSON.parse(response);
    } catch {
      sendDataErrorEle.textContent = 'Invalid JSON response.';
      sendDataErrorEle.classList.add('error');
      return;
    }

    if (!parsedResponse.status) {
      sendDataErrorEle.textContent = parsedResponse.message || 'Server returned an error.';
      sendDataErrorEle.classList.add('error');
      return;
    }

    showsendDataResponseEle.textContent = parsedResponse.message || 'Success!';
    sendDataIptEle.value = '';
  });
});
