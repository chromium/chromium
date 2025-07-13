
// READ DATA
const readDataBtnEle = document.getElementById('readDataBtn');
const showReadDataResponseEle = document.getElementById('showReadDataResponse');

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
const sendDataBtnEle = document.getElementById('sendDataBtn');
const sendDataIptEle = document.getElementById('sendDataIpt');
const sendDataErrorEle = document.getElementById('sendDataError');
const showsendDataResponseEle = document.getElementById('showsendDataResponse');

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

    if (parsedResponse.status == "error") {
      sendDataErrorEle.textContent = parsedResponse.message || 'Server returned an error.';
      sendDataErrorEle.classList.add('error');
      return;
    } else {
      showsendDataResponseEle.textContent = parsedResponse.message || 'Success!';
      // sendDataIptEle.value = '';
    }

  });
});


// LOAD BERT
const loadBertBtnEle = document.getElementById('loadBertBtn');
const showLoadBertResponseEle = document.getElementById('showLoadBertResponse');

loadBertBtnEle.addEventListener('click', () => {
  // Clear previous
  showLoadBertResponseEle.textContent = '';
  showLoadBertResponseEle.classList.remove('error');

  // show message loading
  showLoadBertResponseEle.textContent = 'Loading...';
  showLoadBertResponseEle.classList.add('loading');
  
  chrome.readServerUds.loadModelBERT((response) => {
    
    // removing the loading style
    showLoadBertResponseEle.textContent = '';
    showLoadBertResponseEle.classList.remove('loading');

    if (chrome.runtime.lastError) {
      showLoadBertResponseEle.textContent = 'Native Error: ' + chrome.runtime.lastError.message;
      showLoadBertResponseEle.classList.add('error');
      return;
    }

    let parsedResponse;
    try {
      parsedResponse = JSON.parse(response);
    } catch {
      showLoadBertResponseEle.textContent = 'Invalid JSON response.';
      showLoadBertResponseEle.classList.add('error');
      return;
    }

    if (parsedResponse.status == "error") {
      showLoadBertResponseEle.textContent = parsedResponse.message || 'Server returned an error.';
      showLoadBertResponseEle.classList.add('error');
      return;
    } else {
      showLoadBertResponseEle.textContent = parsedResponse.message || 'Success!';
      showLoadBertResponseEle.classList.remove(['error'])
    }

  });
});


// SINGLE INFERENCE BERT
const singleBertInferBtnEle = document.getElementById('singleBertInferBtn');
const bertQuestionInputEle = document.getElementById('bertQuestionInput');
const bertContextInputEle = document.getElementById('bertContextInput');
const bertInputErrorEle = document.getElementById('bertInputError');
const singleBertInferResponseEle = document.getElementById('singleBertInferResponse');

singleBertInferBtnEle.addEventListener('click', () => {
  const question = bertQuestionInputEle.value.trim();
  const context = bertContextInputEle.value.trim();

  // Clear previous
  bertInputErrorEle.textContent = '';
  bertInputErrorEle.classList.remove('error');
  singleBertInferResponseEle.textContent = '';
  singleBertInferResponseEle.classList.remove('error');

  if (!question || !context) {
    bertInputErrorEle.textContent = 'Please fill both question context before sending.';
    bertInputErrorEle.classList.add('error');
    return;
  }

  const payload = { question: question, context: context };
  const jsonPayload = JSON.stringify(payload);

  // console.log("Bert Infer Paylod", jsonPayload);

  chrome.readServerUds.inferSingleBERT(jsonPayload, (response) => {
    if (chrome.runtime.lastError) {
      bertInputErrorEle.textContent = 'Native Error: ' + chrome.runtime.lastError.message;
      bertInputErrorEle.classList.add('error');
      return;
    }

    let parsedResponse;
    try {
      parsedResponse = JSON.parse(response);
      // console.log(parsedResponse)
    } catch {
      bertInputErrorEle.textContent = 'Invalid JSON response.';
      bertInputErrorEle.classList.add('error');
      return;
    }

    if (parsedResponse.status == "error") {
      bertInputErrorEle.textContent = parsedResponse.message || 'Server returned an error.';
      bertInputErrorEle.classList.add('error');
      return;
    } else {
      singleBertInferResponseEle.textContent = parsedResponse.message || 'Success!';
      // bertQuestionInputEle.value = '';
      // bertContextInputEle.value = '';
    }

  });
});